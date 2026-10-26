################################################################################
1. Download and unzip the kernel source of I610UEU1AYIA.
 
2. Unzip and update the kernel source of I610UEU1AYKE.
 
3. How to Build
        - get Toolchain
                get the proper toolchain packages from AOSP or CodeSourcery or ETC.
                (Download link : https://opensource.samsung.com/uploadSearch?searchValue=toolchain )
                Please decompress in 'kernel_platform' folder
                (toolchain path : kernel_platform\prebuilts, kernel_platform\prebuilts-master)

        - Set and export target config
                1. target config
                        BUILD_TARGET=
                        export MODEL=$(echo ${BUILD_TARGET} | cut -d'_' -f1)
                        export PROJECT_NAME=${MODEL}
                        export REGION=$(echo ${BUILD_TARGET} | cut -d'_' -f2)
                        export CARRIER=$(echo ${BUILD_TARGET} | cut -d'_' -f3)
                        export TARGET_BUILD_VARIANT= user
                        
                2. Chipset common config
                        CHIPSET_NAME=anorak
                        export ANDROID_BUILD_TOP=/home/dpi/qb5_8814/workspace/P4_1716/android/out/target/product/xrvst2
                        export TARGET_PRODUCT=defconfig
                        export TARGET_BOARD_PLATFORM=anorak

                        export KMI_SYMBOL_LIST_STRICT_MODE=0
                        export ANDROID_PRODUCT_OUT=/home/dpi/qb5_8814/workspace/P4_1716/android/out/target/product/
                        export OUT_DIR=/home/dpi/qb5_8814/workspace/P4_1716/android/out/msm-kernel--xrvst2ue
                        export IS_KBUILD="false"

        - Start to trigger build
                3. build kernel
                        RECOMPILE_KERNEL=1 ./kernel_platform/build/android/prepare_vendor.sh sec ${TARGET_PRODUCT}


4. Output files
        - Kernel : arch/${__arch_name}/boot/Image
        - module : drivers/*/*.ko

5. How to Clean
        Change to OUTPUT_DIR folder
        EX) /home/dpi/qb5_8814/workspace/P4_1716/android/out/target/product/xrvst2/out
        $ make clean
################################################################################
