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
#define TABLE_SIZE 15
#define PIECE_SIZE 5
#define ARROW_SIZE 5
#define BLOCK_SIZE 14
#define LINE_SIZE 1
#define LINE_LENGTH ((TABLE_SIZE - 1) * BLOCK_SIZE + TABLE_SIZE * LINE_SIZE)
#define EMPTY_WIDTH ((WINDOW_WIDTH - LINE_LENGTH) / 2)
#define EMPTY_HEIGHT ((WINDOW_HEIGHT - LINE_LENGTH) / 2)
#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3
#define USER 1
#define AI 2
#define TARGET -1

#define WHITE_RGB 255, 255, 255
#define BLACK_RGB 0, 0, 0

unsigned int x = 0, y = 0, direction = UP;
int table[TABLE_SIZE][TABLE_SIZE] = {0};

#define NEXT_DIRECTION direction = (direction + 1) % 4;

void draw_reset()
{
    ILI9341_Draw_Rec_Color(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, BLACK_RGB);
}

void draw_piece(unsigned int x, unsigned int y, int is_user)
{
    if (x <= 14 && y <= 14)
    {
        if (is_user == USER)
        {
            LCD_SetColors(RED, BLACK);
            ILI9341_DrawCircle(EMPTY_WIDTH + x * (BLOCK_SIZE + LINE_SIZE), EMPTY_HEIGHT + y * (BLOCK_SIZE + LINE_SIZE), PIECE_SIZE, 1);
        }
        else if (is_user == AI)
        {
            LCD_SetColors(BLUE, BLACK);
            ILI9341_DrawCircle(EMPTY_WIDTH + x * (BLOCK_SIZE + LINE_SIZE), EMPTY_HEIGHT + y * (BLOCK_SIZE + LINE_SIZE), PIECE_SIZE, 1);
        }
        else if (is_user == TARGET)
        {
            LCD_SetColors(WHITE, BLACK);
            ILI9341_DrawCircle(EMPTY_WIDTH + x * (BLOCK_SIZE + LINE_SIZE), EMPTY_HEIGHT + y * (BLOCK_SIZE + LINE_SIZE), PIECE_SIZE, 1);
        }
    }
}

void draw_arrow()
{
    switch (direction)
    {
    case UP:
        for (int i = 0; i < ARROW_SIZE; ++i)
        {
            ILI9341_Draw_Rec_Color(WINDOW_WIDTH / 2 - i, EMPTY_HEIGHT / 2 - ARROW_SIZE / 2 + i, 2 * i + 1, 1, WHITE_RGB);
        }
        break;
    case RIGHT:
        for (int i = 0; i < ARROW_SIZE; ++i)
        {
            ILI9341_Draw_Rec_Color(WINDOW_WIDTH / 2 + ARROW_SIZE / 2 - i, EMPTY_HEIGHT / 2 - i, 1, 2 * i + 1, WHITE_RGB);
        }
        break;
    case DOWN:
        for (int i = 0; i < ARROW_SIZE; ++i)
        {
            ILI9341_Draw_Rec_Color(WINDOW_WIDTH / 2 - i, EMPTY_HEIGHT / 2 + ARROW_SIZE / 2 - i, 2 * i + 1, 1, WHITE_RGB);
        }
        break;
    case LEFT:
        for (int i = 0; i < ARROW_SIZE; ++i)
        {
            ILI9341_Draw_Rec_Color(WINDOW_WIDTH / 2 - ARROW_SIZE / 2 + i, EMPTY_HEIGHT / 2 - i, 1, 2 * i + 1, WHITE_RGB);
        }
        break;
    }
}

void draw_table()
{
    draw_reset();
    draw_arrow();
    draw_piece(x, y, -1);
    for (int i = EMPTY_WIDTH; i < WINDOW_WIDTH - EMPTY_WIDTH; i += (BLOCK_SIZE + LINE_SIZE))
    {
        ILI9341_Draw_Rec_Color(i, EMPTY_HEIGHT, LINE_SIZE, LINE_LENGTH, WHITE_RGB);
    }
    for (int i = EMPTY_HEIGHT; i < WINDOW_HEIGHT - EMPTY_HEIGHT; i += (BLOCK_SIZE + LINE_SIZE))
    {
        ILI9341_Draw_Rec_Color(EMPTY_WIDTH, i, LINE_LENGTH, LINE_SIZE, WHITE_RGB);
    }
    for (int i = 0; i < TABLE_SIZE; ++i)
    {
        for (int j = 0; j < TABLE_SIZE; ++j)
        {
            draw_piece(i, j, table[i][j]);
        }
    }
}

void move()
{
    int tmpx = x, tmpy = y;
    switch (direction)
    {
    case UP:
        while (tmpy >= 0 && table[x][--tmpy] != 0)
            ;
        if (tmpy >= 0)
        {
            y = tmpy;
        }
        break;
    case RIGHT:
        while (tmpx < TABLE_SIZE && table[++tmpx][y] != 0)
            ;
        if (tmpx < TABLE_SIZE)
        {
            x = tmpx;
        }
        break;
    case DOWN:
        while (tmpy < TABLE_SIZE && table[x][++tmpy] != 0)
            ;
        if (tmpy >= 0)
        {
            y = tmpy;
        }
        break;
    case LEFT:
        while (tmpx >= 0 && table[--tmpx][y] != 0)
            ;
        if (tmpy >= 0)
        {
            x = tmpx;
        }
        break;
    }
}

struct point
{
    int row;
    int column;
    int color;
    int mark;
    int cnt[15];
};

struct point p[17][17];
int player = -1;
int ai = 1;
int rnd = 1;
int bestx = 0;
int besty = 0;
int bestmark = 0;
int firstx = 0;
int firsty = 0;

void init()
{
    for (int i = 1; i <= 15; i++)
        for (int j = 1; j <= 15; j++)
        {
            p[i][j].mark = 0;
            p[i][j].color = 0;
            p[i][j].row = i;
            p[i][j].column = j;
            memset(p[i][j].cnt, 0, sizeof(p[i][j].cnt));
        }
    for (int i = 0; i <= 16; i++)
        for (int j = 0; j <= 16; j++)
            if (i == 0 || j == 0 || i == 16 || j == 16)
                p[i][j].color = 2;
}

int judge(struct point *point)
{
    int row = point->row;
    int column = point->column;
    int color = point->color;
    int countx = 1, county = 1, counts1 = 1, counts2 = 1;
    for (int i = row + 1; i <= row + 4 && i <= 15; i++)
    {
        if (p[i][column].color != color)
            break;
        countx++;
    }
    for (int i = row - 1; i >= row - 4 && i >= 1; i--)
    {
        if (p[i][column].color != color)
            break;
        countx++;
    }
    for (int i = column + 1; i <= column + 4 && i <= 15; i++)
    {
        if (p[row][i].color != color)
            break;
        county++;
    }
    for (int i = column - 1; i >= column - 4 && i >= 1; i--)
    {
        if (p[row][i].color != color)
            break;
        county++;
    }
    for (int i = row + 1, j = column - 1; i <= row + 4 && j >= column - 4 && i <= 15 && j >= 1; i++, j--)
    {
        if (p[i][j].color != color)
            break;
        counts1++;
    }
    for (int i = row - 1, j = column + 1; i >= row - 4 && j <= column + 4 && i >= 1 && j <= 15; i--, j++)
    {
        if (p[i][j].color != color)
            break;
        counts1++;
    }
    for (int i = row + 1, j = column + 1; i <= row + 4 && j <= column + 4 && i <= 15 && j <= 15; i++, j++)
    {
        if (p[i][j].color != color)
            break;
        counts2++;
    }
    for (int i = row - 1, j = column - 1; i >= row - 4 && j >= column - 4 && i >= 1 && j >= 1; i--, j--)
    {
        if (p[i][j].color != color)
            break;
        counts2++;
    }
    if (countx >= 5 || county >= 5 || counts1 >= 5 || counts2 >= 5)
    {
        if (color == player)
            return 1;
        else
            return 2;
    }
    return 0;
}

int down(int setter, struct point *point)
{
    if (rnd == 1)
    {
        firstx = point->row;
        firsty = point->column;
    }
    rnd++;
    if (setter == ai)
        point->color = 1;
    if (setter == player)
        point->color = -1;
    return judge(point);
}

int set_player(int x, int y)
{
    table[x][y] = 1;
    struct point *point = &p[x + 1][y + 1];
    return down(player, point);
}

void find(int dx, int dy, int color)
{
    for (int i = 1; i <= 15; i++)
        for (int j = 1; j <= 15; j++)
        {
            if (p[i][j].color != 0)
                continue;
            int count = 1;
            int block1 = 0;
            int block2 = 0;
            for (int x = i + dx, y = j + dy; x - i <= 4 && x >= 0 && x <= 16 && y - j <= 4 && y >= 0 && y <= 16; x += dx, y += dy)
            {
                if (p[x][y].color == 0)
                    break;
                if (p[x][y].color != color)
                {
                    block1 = 1;
                    break;
                }
                count++;
            }
            for (int x = i - dx, y = j - dy; i - x <= 4 && x >= 0 && x <= 16 && j - y <= 4 && y >= 0 && y <= 16; x -= dx, y -= dy)
            {
                if (p[x][y].color == 0)
                    break;
                if (p[x][y].color != color)
                {
                    block2 = 1;
                    break;
                }
                count++;
            }
            if (color == ai && count >= 5)
                p[i][j].cnt[10]++;
            if (color == player && count >= 5)
                p[i][j].cnt[9]++;
            if (color == ai && count == 4 && !block1 && !block2)
                p[i][j].cnt[8]++;
            if (color == ai && count == 4 && ((!block1 && block2) || (block1 && !block2)))
                p[i][j].cnt[7]++;
            if (color == player && count == 4 && !block1 && !block2)
                p[i][j].cnt[6]++;
            if (color == ai && count == 3 && !block1 && !block2)
                p[i][j].cnt[5]++;
            if (color == player && count == 4 && ((!block1 && block2) || (block1 && !block2)))
                p[i][j].cnt[4]++;
            if (color == player && count == 3 && !block1 && !block2)
                p[i][j].cnt[3]++;
            if (color == ai && count == 2 && !block1 && !block2)
                p[i][j].cnt[2]++;
            if (color == ai && count == 2 && ((!block1 && block2) || (block1 && !block2)))
                p[i][j].cnt[1]++;
        }
}

void evaluate()
{
    for (int i = 1; i <= 15; i++)
    {
        for (int j = 1; j <= 15; j++)
        {
            p[i][j].mark = p[i][j].mark + p[i][j].cnt[1] + p[i][j].cnt[2] * 10 + p[i][j].cnt[6] * 1e6 + p[i][j].cnt[8] * 1e7 + p[i][j].cnt[9] * 1e8 + p[i][j].cnt[10] * 1e9;
            if (p[i][j].cnt[3] <= 1)
                p[i][j].mark += p[i][j].cnt[3] * 1000;
            else
                p[i][j].mark += 1e6;

            if (p[i][j].cnt[4] <= 1)
                p[i][j].mark += p[i][j].cnt[4] * 100;
            else
                p[i][j].mark += 1e6;

            if (p[i][j].cnt[5] <= 1)
                p[i][j].mark += p[i][j].cnt[5] * 1e4;
            else
                p[i][j].mark += 1e5;

            if (p[i][j].cnt[7] <= 1)
                p[i][j].mark += p[i][j].cnt[7] * 5000;
            else
                p[i][j].mark += 1e7;

            if (p[i][j].cnt[4] && p[i][j].cnt[3])
                p[i][j].mark += 5e5;
            if (p[i][j].cnt[5] && p[i][j].cnt[7])
                p[i][j].mark += 1e7;

            if (p[i][j].mark > bestmark)
            {
                bestx = i;
                besty = j;
                bestmark = p[i][j].mark;
            }
        }
    }
}

int set_ai()
{

    for (int i = 1; i <= 15; i++)
        for (int j = 1; j <= 15; j++)
        {
            p[i][j].mark = 0;
            memset(p[i][j].cnt, 0, sizeof(p[i][j].cnt));
        }
    bestmark = 0;
    if (rnd == 2 && ai == 1)
    {
        int a = 0, b = 0;
        int flag = 1;
        srand(0);
        while (flag)
        {
            a = rand() % 3 - 1;
            b = rand() % 3 - 1;
            if ((a != 0 || b != 0) && p[firstx + a][firsty + b].color != 2)
                flag = 0;
        }
        table[firstx + a - 1][firsty + b - 1] = 2;
        return down(ai, &p[firstx + a][firsty + b]);
    }
    find(1, 0, -1);
    find(1, 0, 1);
    find(0, 1, -1);
    find(0, 1, 1);
    find(1, 1, -1);
    find(1, 1, 1);
    find(1, -1, -1);
    find(1, -1, 1);
    evaluate();
    table[bestx - 1][besty - 1] = 2;
    return down(ai, &p[bestx][besty]);
}

int main(void)
{
    unsigned long t0 = 0, t1 = 0;

    Key_GPIO_Config();

    ILI9341_Init(); // LCD ³õÊ¼»¯
    ILI9341_GramScan(6);
    LCD_SetFont(&Font8x16);
    LCD_SetColors(RED, BLACK);

    SysTick_Init();
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    /* LED ¶Ë¿Ú³õÊ¼»¯ */
    LED_GPIO_Config();

    /* ´®¿ÚÍ¨ÐÅ³õÊ¼»¯ */
    USART_Config();

    // I2C³õÊ¼»¯
    I2C_Bus_Init();
    ILI9341_Clear(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    init();
    draw_table();
    while (1)
    {
        if (Key_Scan(KEY1_GPIO_PORT, KEY1_GPIO_PIN) == KEY_ON)
        {
            move();
            draw_table();
        }
        else if (Key_Scan(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == KEY_ON)
        {
            get_tick_count(&t0);
            t1 = t0;
            while (t1 - t0 <= 500)
            {
                if (Key_Scan(KEY2_GPIO_PORT, KEY2_GPIO_PIN) == KEY_ON)
                {
                    if (set_player(x, y) == 1)
                    {
                        LED_RED;
                        return 0;
                    }
                    if (set_ai() == 2)
                    {
                        LED_BLUE;
                        return 0;
                    }
                    move();
                    goto end;
                }
                else
                {
                    get_tick_count(&t1);
                }
            }
            NEXT_DIRECTION;
        end:
            draw_table();
        }
    }
}
