#ifndef SEC_ABC_KUNIT_H
#define SEC_ABC_KUNIT_H
#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/sti/abc_common.h>
#include <linux/sti/abc_hub.h>
#include <linux/sti/abc_spec_manager.h>
#include <linux/platform_device_mock.h>

#define ABC_TEST_UEVENT_MAX		10
#define TIME_KEYWORD		"TIMESTAMP="
#define ABC_WAIT_QUEUE_WORK_TIMEOUT	20

__visible_for_testing
ssize_t enabled_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count);
__visible_for_testing
ssize_t enabled_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf);
__visible_for_testing
ssize_t enable_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf,
					 size_t count);
__visible_for_testing
ssize_t bootc_offset_store(struct device *dev,
						   struct device_attribute *attr,
						   const char *buf,
						   size_t count);
__visible_for_testing
ssize_t bootc_time_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf,
						 size_t count);
__visible_for_testing
ssize_t enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);
__visible_for_testing
ssize_t bootc_offset_show(struct device *dev,
						  struct device_attribute *attr,
						  char *buf);
__visible_for_testing
ssize_t bootc_time_show(struct device *dev,
						struct device_attribute *attr,
						char *buf);
__visible_for_testing
ssize_t spec_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf,
				   size_t count);
__visible_for_testing
ssize_t spec_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf);
__visible_for_testing
ssize_t features_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf,
					   size_t count);
__visible_for_testing
ssize_t features_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);
__visible_for_testing
void sec_abc_free_allocated_memory(void);
__visible_for_testing
int sec_abc_get_idx_of_registered_event(char *module_name, char *error_name);
__visible_for_testing
int abc_alloc_memory_to_buffer_type1(struct spec_data_type1 *spec_type1, int max_cnt);
__visible_for_testing
void sec_abc_send_uevent(struct abc_key_data *key_data, char *uevent_type);
__visible_for_testing
int sec_abc_clear_pre_events(void);
__visible_for_testing
struct abc_pre_event *sec_abc_get_pre_event_node(struct abc_key_data *key_data);
__visible_for_testing
void sec_abc_process_changed_enable_mode(void);
__visible_for_testing
int sec_abc_process_pre_events(void);
__visible_for_testing
void sec_abc_init_work(struct abc_info *pinfo);
__visible_for_testing
int sec_abc_get_buffer_size_from_th_cnt(int th_max);
__visible_for_testing
int sec_abc_get_registered_abc_event_total(void);
unsigned int sec_abc_get_ktime_ms(void);
void sec_abc_free_pre_events(void);
struct abc_common_spec_data *sec_abc_get_matched_common_spec_type1(char *module_name, char *error_name);
int abc_hub_bootc_get_total_offset(struct sub_bootc_pdata *bootc_pdata);
void abc_common_test_get_work_str(char **uevent_str);
void abc_common_test_get_log_str(char *str);
void abc_common_test_init_uevent_str(void);
void abc_common_test_init_variables(void);
void abc_common_test_set_uml(struct kunit *test);
int abc_hub_bootc_init_work(void);
#define ABC_HUB_TEST_STR_MAX	50

extern char abc_hub_kunit_test_uevent_str[ABC_HUB_TEST_STR_MAX];
extern char abc_common_kunit_test_work_str[ABC_TEST_UEVENT_MAX][ABC_TEST_STR_MAX];
extern char sec_abc_kunit_test_log_str[ABC_TEST_STR_MAX];
extern char test_string_buf[ABC_BUFFER_MAX];
extern char test_spec_string_buf[PAGE_SIZE];
extern char test_event_str[ABC_BUFFER_MAX];
extern char test_spec_string[PAGE_SIZE];
extern char module_buf[ABC_BUFFER_MAX];
extern char error_buf[ABC_BUFFER_MAX];
extern char *test_uevent_str[2];
extern char bootc_offset_module[BOOTC_OFFSET_DATA_CNT][BOOTC_OFFSET_STR_MAX];
extern int test_int_buf;
extern unsigned long test_timeout;
extern struct abc_common_spec_data *test_common_spec;
extern struct spec_data_type1 *test_spec_type1;
extern struct abc_pre_event *test_pre_event;
extern struct abc_key_data test_key_data;
extern struct list_head abc_pre_event_list;
extern struct device *abc_hub_dev;
extern struct device *sec_abc;
extern struct abc_info *pinfo;
extern int abc_enable_mode;
extern int abc_pre_event_cnt;
extern int abc_hub_probed;
extern int abc_init;
extern bool abc_save_pre_event;
struct test_spec_list {
	char module_name[ABC_EVENT_STR_MAX];
	char error_name[ABC_EVENT_STR_MAX];
	int threshold_cnt;
	int threshold_time;
};
#endif
