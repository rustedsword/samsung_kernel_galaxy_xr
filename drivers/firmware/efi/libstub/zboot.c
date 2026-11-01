// SPDX-License-Identifier: GPL-2.0

#include <linux/efi.h>
#include <linux/pe.h>
#include <asm/efi.h>
#include <asm/unaligned.h>

#include "efistub.h"

static unsigned char zboot_heap[SZ_256K] __aligned(64);
static unsigned long free_mem_ptr, free_mem_end_ptr;

#define STATIC static
#if defined(CONFIG_KERNEL_GZIP)
#include "../../../../lib/decompress_inflate.c"
#elif defined(CONFIG_KERNEL_LZ4)
#include "../../../../lib/decompress_unlz4.c"
#elif defined(CONFIG_KERNEL_LZMA)
#include "../../../../lib/decompress_unlzma.c"
#elif defined(CONFIG_KERNEL_LZO)
#include "../../../../lib/decompress_unlzo.c"
#elif defined(CONFIG_KERNEL_XZ)
#undef memcpy
#define memcpy memcpy
#undef memmove
#define memmove memmove
#include "../../../../lib/decompress_unxz.c"
#elif defined(CONFIG_KERNEL_ZSTD)
#include "../../../../lib/decompress_unzstd.c"
#endif

extern char _gzdata_start[], _gzdata_end[];

static void log(efi_char16_t str[])
{
	efi_call_proto(efi_table_attr(efi_system_table, con_out),
		       output_string, L"EFI decompressor: ");
	efi_call_proto(efi_table_attr(efi_system_table, con_out),
		       output_string, str);
	efi_call_proto(efi_table_attr(efi_system_table, con_out),
		       output_string, L"\n");
}

static void error(char *x)
{
	log(L"error() called from decompressor library\n");
}

asmlinkage efi_status_t __efiapi
efi_zboot_entry(efi_handle_t handle, efi_system_table_t *systab)
{
	unsigned long compressed_size = _gzdata_end - _gzdata_start;
	efi_loaded_image_t *parent, *child;
	unsigned long exit_data_size;
	efi_handle_t child_handle;
	efi_char16_t *exit_data;
	efi_status_t status;
	unsigned long size;
	void *image;
	int ret;

	WRITE_ONCE(efi_system_table, systab);

	free_mem_ptr = (unsigned long)&zboot_heap;
	free_mem_end_ptr = free_mem_ptr + sizeof(zboot_heap);

	exit_data = NULL;
	exit_data_size = 0;

	status = efi_bs_call(handle_protocol, handle,
			     &LOADED_IMAGE_PROTOCOL_GUID, (void **)&parent);
	if (status != EFI_SUCCESS) {
		log(L"Failed to locate parent's loaded image protocol");
		return status;
	}

	// The uncompressed size of the payload is appended to the raw bit
	// stream, and may therefore appear misaligned in memory
	size = get_unaligned_le32(_gzdata_end - 4);
	status = efi_bs_call(allocate_pool, EFI_LOADER_DATA, size, &image);
	if (status != EFI_SUCCESS) {
		log(L"Failed to allocate memory for the decompressed image");
		return status;
	}

	ret = __decompress(_gzdata_start, compressed_size, NULL, NULL,
			   image, size, NULL, error);
	if (ret < 0) {
		log(L"Decompression failed");
		efi_bs_call(free_pool, image);
		return EFI_DEVICE_ERROR;
	}

	// Before EndOfDxe, the firmware refuses images with a device path
	child_handle = NULL;
	status = efi_bs_call(load_image, false, handle, NULL, image, size,
			     &child_handle);
	efi_bs_call(free_pool, image);
	if (status != EFI_SUCCESS) {
		log(L"Failed to load image");
		if (status == EFI_SECURITY_VIOLATION && child_handle != NULL)
			efi_bs_call(unload_image, child_handle);
		goto exit;
	}

	status = efi_bs_call(handle_protocol, child_handle,
			     &LOADED_IMAGE_PROTOCOL_GUID, (void **)&child);
	if (status != EFI_SUCCESS) {
		log(L"Failed to locate child's loaded image protocol");
		efi_bs_call(unload_image, child_handle);
		goto exit;
	}

	// Copy the kernel command line and the boot device
	child->load_options = parent->load_options;
	child->load_options_size = parent->load_options_size;
	child->device_handle = parent->device_handle;

	status = efi_bs_call(start_image, child_handle, &exit_data_size,
			     &exit_data);
	if (status != EFI_SUCCESS) {
		log(L"StartImage() returned with error");
		if (exit_data_size > 0)
			log(exit_data);

		// If StartImage() returns EFI_SECURITY_VIOLATION, the image is
		// not unloaded so we need to do it by hand.
		if (status == EFI_SECURITY_VIOLATION)
			efi_bs_call(unload_image, child_handle);
	}

exit:
	efi_bs_call(exit, handle, status, exit_data_size, exit_data);

	// Free ExitData in case Exit() returned with a failure code,
	// but return the original status code.
	log(L"Exit() returned with failure code");
	if (exit_data != NULL)
		efi_bs_call(free_pool, exit_data);
	return status;
}
