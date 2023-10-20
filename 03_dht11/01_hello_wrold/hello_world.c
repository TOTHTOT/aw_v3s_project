/*
 * @Description:
 * @Author: TOTHTOT
 * @Date: 2023-10-18 11:21:11
 * @LastEditTime: 2023-10-20 14:03:51
 * @LastEditors: TOTHTOT
 * @FilePath: /u8g2-arm-linux/examples/c-examples/01_hello_wrold/hello_world.c
 */
#include <u8g2port.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#include <linux/input.h>
#include <signal.h>
#include <sys/epoll.h>

// Set I2C bus and address
#define I2C_BUS 0
#define I2C_ADDRESS 0x3c * 2

#define DHT11_PATH "/dev/dht11"

#define KEY_PATH "/dev/input/event0"
#define KEY_PRESS 1	  // 按键按下
#define KEY_RELEASE 0 // 按键松开

// dht11 传感器数据类型
typedef struct dht11_data
{
	uint16_t temp;
	uint16_t humi;
} dht11_data_t;
typedef struct u8g2_test
{
	u8g2_t *u8g2_p;
	int key_fd;
	int dht11_fd;
	int epfd;								// epoll的文件描述符
#define EP_EVENT_MAX 2						// epoll监听事件最大值 按键, dht11
	struct epoll_event event[EP_EVENT_MAX]; // epoll 监听事件数组
	uint8_t exit_flag;						// 程序推出标志
	pthread_t pro_bar_tid;					// 存储新线程的 ID
	struct input_event key_ipt_ent_st;		// 按键的 input 子系统
	struct key_state
	{
		uint16_t num;
		uint8_t state;
	} key_state_st;
	dht11_data_t dht11_sensor_data;

	pthread_mutex_t dht_rw_mut; // dht11 传感器数据互斥锁保证数据稳定
} u8g2_test_t;
u8g2_test_t *g_test_st_p;

/**
 * @name: pthread_process_bar
 * @msg: 进度条线程
 * @param {void} *arg 传递的参数
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-20 10:09:27
 */
void *pthread_process_bar(void *arg)
{
	u8g2_test_t *test_p_st = (u8g2_test_t *)arg;
	int32_t x = 10;
	int32_t y = 30;
	int32_t w = x;
	int32_t w_max = 100;
	int32_t h = 10;
	char buf[40] = {0};

	while (test_p_st->exit_flag == 0)
	{
		u8g2_ClearBuffer(test_p_st->u8g2_p);
		sprintf(buf, "%3d%%", w);
		u8g2_DrawRFrame(test_p_st->u8g2_p, x, y, w_max, h, 4); // 圆角空心矩形, 做边框
		u8g2_DrawRBox(test_p_st->u8g2_p, x, y, w, h, 4);	   // 随进度填充的圆角实心圆角矩形
		u8g2_SetFont(test_p_st->u8g2_p, u8g2_font_unifont_t_symbols);
		u8g2_DrawStr(test_p_st->u8g2_p, 10, 60, buf);

		u8g2_DrawGlyph(test_p_st->u8g2_p, 112, 56, 0x2606);

		if (w > 100)
		{
			w = x;
		}
		w++;
		pthread_mutex_lock(&test_p_st->dht_rw_mut);
		u8g2_SetFont(test_p_st->u8g2_p, u8g2_font_unifont_t_symbols);
		sprintf(buf, "T:%d H:%d", test_p_st->dht11_sensor_data.temp / 10, test_p_st->dht11_sensor_data.humi / 10);
		u8g2_DrawStr(test_p_st->u8g2_p, 0, 18, buf);
		pthread_mutex_unlock(&test_p_st->dht_rw_mut);
		if (test_p_st->key_state_st.state == KEY_PRESS)
		{
			sprintf(buf, "key %d", test_p_st->key_state_st.num);
			u8g2_SetFont(test_p_st->u8g2_p, u8g2_font_logisoso20_tr);
			u8g2_DrawStr(test_p_st->u8g2_p, 60, 18, buf);
		}
		u8g2_SendBuffer(test_p_st->u8g2_p);
		usleep(50000); // 50ms
	}

	return NULL;
}

/**
 * @name: signal_handler
 * @msg: 信号处理函数
 * @param {int} sig
 * @return {*}
 * @author: TOTHTOT
 * @Date: 2023-10-19 17:36:53
 */
void signal_handler(int sig)
{
	switch (sig)
	{
	case SIGINT:
		g_test_st_p->exit_flag = 1;
		printf("app exit\n");
		break;

	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	u8g2_test_t test_t = {0};
	u8g2_t u8g2;
	int32_t ret = 0;
	int32_t ep_ret = 0; // epoll 返回值

	// 指针赋值
	test_t.u8g2_p = &u8g2;
	g_test_st_p = &test_t;

	// Initialization
	u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
										   u8x8_byte_arm_linux_hw_i2c, u8x8_arm_linux_gpio_and_delay);
	init_i2c_hw(&u8g2, I2C_BUS);
	u8g2_SetI2CAddress(&u8g2, I2C_ADDRESS);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);

	test_t.key_fd = open(KEY_PATH, O_RDONLY);
	if (test_t.key_fd < 0)
	{
		ret = 1;
		goto exit_app;
	}
	test_t.dht11_fd = open(DHT11_PATH, O_RDONLY);
	if (test_t.key_fd < 0)
	{
		ret = 1;
		close(test_t.key_fd);
		goto exit_app;
	}

	printf("dht11 fd = %d, key fd = %d\n", test_t.dht11_fd, test_t.key_fd);
	struct epoll_event event;
	event.events = EPOLLIN;		   // 监控读事件
	event.data.fd = test_t.key_fd; // 文件描述符
	test_t.epfd = epoll_create(1);
	if (epoll_ctl(test_t.epfd, EPOLL_CTL_ADD, test_t.key_fd, &event) == -1)
	{
		perror("epoll_ctl key");
		ret = 2;
		goto exit_app;
	}

	event.events = EPOLLIN;			 // 监控读事件
	event.data.fd = test_t.dht11_fd; // 文件描述符
	if (epoll_ctl(test_t.epfd, EPOLL_CTL_ADD, test_t.dht11_fd, &event) == -1)
	{
		perror("epoll_ctl dht11");
		ret = 2;
		goto exit_app;
	}

	// 监听信号
	signal(SIGINT, signal_handler);

	// 初始化互斥锁
	pthread_mutex_init(&test_t.dht_rw_mut, NULL);
	// 创建线程
	pthread_create(&test_t.pro_bar_tid, NULL, pthread_process_bar, &test_t);

	while (test_t.exit_flag == 0)
	{
		ep_ret = epoll_wait(test_t.epfd, test_t.event, EP_EVENT_MAX, 0); // 等待事件
		if (ep_ret == -1)
		{
			perror("epoll_wait");
			test_t.exit_flag = 1;
			ret = 3;
			goto wait_thread_app;
		}
		if (ep_ret > 0) // 事件发生
		{
			int i = 0;
			for (i = 0; i < ep_ret; i++) // 遍历事件
			{
				if (test_t.event[i].events == EPOLLIN && test_t.event[i].data.fd == test_t.key_fd) // 找到匹配事件
				{
					read(test_t.key_fd, &test_t.key_ipt_ent_st, sizeof(struct input_event));
					if (test_t.key_ipt_ent_st.type == EV_KEY)
					{
						test_t.key_state_st.num = test_t.key_ipt_ent_st.code;
						test_t.key_state_st.state = test_t.key_ipt_ent_st.value;
						printf("Event type: %d, code: %d, value: %d\n", test_t.key_ipt_ent_st.type, test_t.key_ipt_ent_st.code, test_t.key_ipt_ent_st.value);
					}
				}
				else if (test_t.event[i].events == EPOLLIN && test_t.event[i].data.fd == test_t.dht11_fd) // 找到匹配事件
				{
					pthread_mutex_lock(&test_t.dht_rw_mut);
					ret = read(test_t.dht11_fd, &test_t.dht11_sensor_data, sizeof(dht11_data_t));
					pthread_mutex_unlock(&test_t.dht_rw_mut);
					if (ret < 0)
					{
						printf("read() fail[%d]\n", ret);
					}
					else
					{
						printf("dht11 data{temp = %d, humi = %d}\n", test_t.dht11_sensor_data.temp / 10, test_t.dht11_sensor_data.humi / 10);
					}
				}
				else
				{
					printf("unknow events\n");
				}
			}
		}
		usleep(50000); // 50ms
	}

	printf("wait pthread exit\n");
	pthread_join(test_t.pro_bar_tid, NULL);
	printf("exit\n");
	u8g2_SetPowerSave(&u8g2, 1);
	done_i2c();
	// 销毁互斥锁
	pthread_mutex_destroy(&test_t.dht_rw_mut);

	close(test_t.key_fd);
	close(test_t.dht11_fd);
	return 0;

wait_thread_app:
	printf("err: wait pthread exit\n");
	pthread_join(test_t.pro_bar_tid, NULL);
	// 销毁互斥锁
	pthread_mutex_destroy(&test_t.dht_rw_mut);
	printf("err:exit\n");
exit_app:
	u8g2_SetPowerSave(&u8g2, 1);
	// Close and deallocate i2c_t
	done_i2c();
	// Close and deallocate GPIO resources
	done_user_data(&u8g2);
	printf("ret = %d\n", ret);

	return ret;
}
