#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include "./systick/bsp_SysTick.h"
#include "./led/bsp_led.h"
#include "./usart/bsp_usart.h"
#include "./i2c/bsp_i2c.h"
#include "./exti/bsp_exti.h"
#include "./key/bsp_key.h"
#include "./lcd/bsp_ili9341_lcd.h"

#include "log.h"
#include "packet.h"


#define WINDOW_WIDTH 240
#define WINDOW_HEIGHT 320
#define N 29
#define M 24
#define ROOM_SIZE 10
#define WALL_SIZE 2
struct edge
{
	int ux, uy, vx, vy;
	int dir;
};

int fa[N * M];
char maze_map[N * 2 + 1][M * 2 + 1];
struct edge edges[N * (M - 1) + M * (N - 1)];
int cnt_e = 0;
int getfa(int x)
{
	if (fa[x] == x)
		return x;
	return fa[x] = getfa(fa[x]);
}

void maze_init()
{
	memset(maze_map, '#', sizeof(maze_map));
	for (int i = 0; i < N; i++)
		for (int j = 0; j < M; j++)
		{
			fa[i * M + j] = i * M + j;
			maze_map[i * 2 + 1][j * 2 + 1] = ' ';
		}
	for (int i = 0; i < N; i++)
		for (int j = 0; j < M - 1; j++)
		{
			edges[cnt_e].ux = i;
			edges[cnt_e].uy = j;
			edges[cnt_e].vx = i;
			edges[cnt_e].vy = j + 1;
			maze_map[i * 2 + 1][j * 2 + 2] = '#';
			edges[cnt_e].dir = 0;
			cnt_e++;
		}
	for (int j = 0; j < M; j++)
		for (int i = 0; i < N - 1; i++)
		{
			edges[cnt_e].ux = i;
			edges[cnt_e].uy = j;
			edges[cnt_e].vx = i + 1;
			edges[cnt_e].vy = j;
			maze_map[i * 2 + 2][j * 2 + 1] = '#';
			edges[cnt_e].dir = 1;
			cnt_e++;
		}

	while (cnt_e)
	{
		int x = rand() % cnt_e;
		int fa_v = getfa(edges[x].vx * M + edges[x].vy);
		int fa_u = getfa(edges[x].ux * M + edges[x].uy);
		if (fa_v != fa_u)
		{
			fa[fa_v] = fa_u;
			if (edges[x].dir == 0)
				maze_map[edges[x].ux * 2 + 1][edges[x].uy * 2 + 2] = ' ';
			else
				maze_map[edges[x].ux * 2 + 2][edges[x].uy * 2 + 1] = ' ';
		}
		for (int i = x + 1; i < cnt_e; i++)
			edges[i - 1] = edges[i];
		cnt_e--;
	}
}
#define POINT_SIZE 4
int now_x = POINT_SIZE + WALL_SIZE;
int now_y = POINT_SIZE + WALL_SIZE;
int dx[] = {0, 1, 0, -1};
int dy[] = {1, 0, -1, 0};
int dir = 0;
int end_x = M*ROOM_SIZE-POINT_SIZE-WALL_SIZE;
int end_y = N*ROOM_SIZE-POINT_SIZE-WALL_SIZE;
void draw_map()
{
	int clr_x = now_x - POINT_SIZE-1;
	int clr_y = now_y - POINT_SIZE-1;
	ILI9341_Draw_Rec_Color(clr_x > 0 ? clr_x : 0, clr_y > 0 ? clr_y : 0, POINT_SIZE * 2+2, POINT_SIZE * 2+2, 0, 0, 0);
	LCD_SetColors(RED, BLACK);
	ILI9341_DrawCircle(now_x, now_y, POINT_SIZE/2, 1);
	for (int i = 0; i < N * 2 + 1; i++)
	{
		int dir = (i + 1) & 1;
		for (int j = dir; j < M * 2 + 1; j += 2)
		{
			if (maze_map[i][j] == '#')
			{
				if (dir == 1)
					ILI9341_Draw_Rec_Color(ROOM_SIZE * (j / 2), ROOM_SIZE * (i / 2), ROOM_SIZE, WALL_SIZE, 255, 255, 255);
				else
					ILI9341_Draw_Rec_Color(ROOM_SIZE * (j / 2), ROOM_SIZE * (i / 2), WALL_SIZE, ROOM_SIZE, 255, 255, 255);
			}
		}
	}
	ILI9341_Draw_Rec_Color(end_x,end_y, POINT_SIZE, POINT_SIZE, 0, 255, 0);
	
}
int dirs[4][5][5] = {{0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0}, {0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0}};
#define DIR_SIZE 3
void show_dir(int dir)
{
	for (int i = 0; i < 5; i++)
		for (int j = 0; j < 5; j++)
		{
			uint8_t c = dirs[dir][i][j] == 1 ? 255 : 0;
			ILI9341_Draw_Rec_Color(110 + i * DIR_SIZE, 300 + j * DIR_SIZE, DIR_SIZE, DIR_SIZE, c, c, c);
		}
}

void reset()
{
	now_x = POINT_SIZE + WALL_SIZE;
	now_y = POINT_SIZE + WALL_SIZE;
	dir = 0;
	ILI9341_Clear(0, 0, 240, 320);
	maze_init();
	draw_map();
	show_dir(dir);
}
void move(int dir)
{
	if (now_x + dx[dir] >= 0 && now_x + dx[dir] <= WINDOW_WIDTH && now_y + dy[dir] >= 0 && now_y + dy[dir] <= WINDOW_HEIGHT)
	{
		printf("%d %d %d\n", now_x + dx[dir], now_y + dy[dir], ILI9341_GetPointPixel(now_x + dx[dir], now_y + dy[dir]));
		if (ILI9341_GetPointPixel(now_x + dx[dir], now_y + dy[dir]) != WHITE)
		{
			now_x = now_x + dx[dir];
			now_y = now_y + dy[dir];
			draw_map();
			show_dir(dir);
		}
	}
	
	if((now_x-end_x)*(now_x-end_x)+(now_y-end_y)*(now_y-end_y)<=POINT_SIZE*POINT_SIZE)
	{
		reset();
	}
}

int main(void)
{
	Key_GPIO_Config();
	
    ILI9341_Init();
    ILI9341_GramScan(6);
    LCD_SetFont(&Font8x16);
    LCD_SetColors(RED, BLACK);

    SysTick_Init();
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    LED_GPIO_Config();
    LED_BLUE;

    USART_Config();

    EXTI_Pxy_Config();
    I2C_Bus_Init();
    ILI9341_Clear(0, 0, 240, 320);
	
	maze_init();
	draw_map();
	show_dir(dir);
	
    while (1)
    {
		if (Key_Scan(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == KEY_ON)
		{
			dir = (dir + 1) % 4;
			show_dir(dir);
		}
		if (Key_Scan(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == KEY_ON)
		{
			move(dir);
		}
    }
}
