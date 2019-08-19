//----------------------------------------------------------------------------------
//		局所領域選択・平均化フィルタ
//----------------------------------------------------------------------------------
#include <windows.h>
#include "filter.h"
#include "version.h"
#define YCP_EDIT(fpip,x,y) ((fpip)->ycp_edit+(x)+(fpip)->max_w*(y))
#define YCP_TEMP(fpip,x,y) ((fpip)->ycp_temp+(x)+(fpip)->max_w*(y))
#define NR 25
#define NA 45
#define NL 25
#define NI 9

//相対座標リスト
static signed char rels[NR][2] = {
	{-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2},
	{-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1},
	{-2, 0}, {-1, 0}, {0, 0}, {1, 0}, {2, 0},
	{-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1},
	{-2, 2}, {-1, 2}, {0, 2}, {1, 2}, {2, 2},
};

//領域(relsの指数の集合)リスト
static char areas[NA][NI] = {
	{ 0,  1,  5,  6,  7, 11, 12, NR, NR},
	{ 1,  2,  3,  6,  7,  8, 12, NR, NR},
	{ 3,  4,  7,  8,  9, 12, 13, NR, NR},
	{ 5,  6, 10, 11, 12, 15, 16, NR, NR},
	{ 6,  7,  8, 11, 12, 13, 16, 17, 18},
	{ 8,  9, 12, 13, 14, 18, 19, NR, NR},
	{11, 12, 15, 16, 17, 20, 21, NR, NR},
	{12, 16, 17, 18, 21, 22, 23, NR, NR},
	{12, 13, 17, 18, 19, 23, 24, NR, NR},
	{10, 11, 12, 15, 16, NR, NR, NR, NR},
	{12, 15, 16, 20, 21, NR, NR, NR, NR},
	{12, 17, 21, 22, 23, NR, NR, NR, NR},
	{12, 18, 19, 23, 24, NR, NR, NR, NR},
	{12, 13, 14, 18, 19, NR, NR, NR, NR},
	{11, 12, 13, 16, 17, 18, NR, NR, NR},
	{ 2,  3,  7,  8, 12, NR, NR, NR, NR},
	{ 3,  4,  8,  9, 12, NR, NR, NR, NR},
	{ 9, 12, 13, 14, 19, NR, NR, NR, NR},
	{12, 17, 18, 22, 23, NR, NR, NR, NR},
	{ 7,  8, 12, 13, 17, 18, NR, NR, NR},
	{ 1,  2,  6,  7, 12, NR, NR, NR, NR},
	{ 0,  1,  5,  6, 12, NR, NR, NR, NR},
	{ 5, 10, 11, 12, 15, NR, NR, NR, NR},
	{12, 16, 17, 21, 22, NR, NR, NR, NR},
	{ 6,  7, 11, 12, 16, 17, NR, NR, NR},
	{ 5,  6, 10, 11, 12, NR, NR, NR, NR},
	{ 1,  2,  3,  7, 12, NR, NR, NR, NR},
	{ 8,  9, 12, 13, 14, NR, NR, NR, NR},
	{ 6,  7,  8, 11, 12, 13, NR, NR, NR},
	{12, 13, 17, 18, NR, NR, NR, NR, NR},
	{12, 17, 22, 23, NR, NR, NR, NR, NR},
	{12, 18, 24, NR, NR, NR, NR, NR, NR},
	{12, 13, 14, 18, NR, NR, NR, NR, NR},
	{11, 12, 16, 17, NR, NR, NR, NR, NR},
	{10, 11, 12, 15, NR, NR, NR, NR, NR},
	{12, 16, 20, NR, NR, NR, NR, NR, NR},
	{12, 17, 21, 22, NR, NR, NR, NR, NR},
	{ 7,  8, 12, 13, NR, NR, NR, NR, NR},
	{ 2,  3,  7, 12, NR, NR, NR, NR, NR},
	{ 4,  8, 12, NR, NR, NR, NR, NR, NR},
	{ 9, 12, 13, 14, NR, NR, NR, NR, NR},
	{ 6,  7, 11, 12, NR, NR, NR, NR, NR},
	{ 5, 10, 11, 12, NR, NR, NR, NR, NR},
	{ 0,  6, 12, NR, NR, NR, NR, NR, NR},
	{ 1,  2,  7, 12, NR, NR, NR, NR, NR},
};

//領域選択肢(areasの指数の集合)リスト
static char locals[NL][NI] = {            // y, x
	{29, 30, 31, 32, NA, NA, NA, NA, NA}, // 0, 0
	{12, 13, 14, 18, 23, NA, NA, NA, NA}, // 0, 1
	{ 9, 10, 11, 12, 13, 14, NA, NA, NA}, // 0, *
	{ 9, 10, 14, 18, 28, NA, NA, NA, NA}, // 0,-2
	{33, 34, 35, 36, NA, NA, NA, NA, NA}, // 0,-1
	{13, 15, 16, 19, 27, NA, NA, NA, NA}, // 1, 0
	{ 4,  5,  7,  8, NA, NA, NA, NA, NA}, // 1, 1
	{ 3,  4,  5,  6,  7,  8, NA, NA, NA}, // 1, *
	{ 3,  4,  6,  7, NA, NA, NA, NA, NA}, // 1,-2
	{ 9, 10, 23, 24, 25, NA, NA, NA, NA}, // 1,-1
	{12, 15, 16, 17, 18, 19, NA, NA, NA}, // *, 0
	{ 1,  2,  4,  5,  7,  8, NA, NA, NA}, // *, 1
	{ 0,  1,  2,  3,  4,  5,  6,  7,  8}, // *, *
	{ 0,  1,  3,  4,  6,  7, NA, NA, NA}, // *,-2
	{10, 20, 21, 22, 23, 24, NA, NA, NA}, // *,-1
	{13, 15, 16, 19, 27, NA, NA, NA, NA}, //-2, 0
	{ 1,  2,  4,  5, NA, NA, NA, NA, NA}, //-2, 1
	{ 0,  1,  2,  3,  4,  5, NA, NA, NA}, //-2, *
	{ 0,  1,  2,  4,  5, NA, NA, NA, NA}, //-2,-2
	{ 9, 20, 21, 24, 25, NA, NA, NA, NA}, //-2,-1
	{37, 38, 39, 40, NA, NA, NA, NA, NA}, //-1, 0
	{15, 16, 20, 27, 28, NA, NA, NA, NA}, //-1, 1
	{16, 21, 25, 26, 27, 28, NA, NA, NA}, //-1, *
	{15, 16, 20, 27, 28, NA, NA, NA, NA}, //-1,-2
	{41, 42, 43, 44, NA, NA, NA, NA, NA}, //-1,-1
};

static void set_smoothed(FILTER *fp, FILTER_PROC_INFO *fpip, int local_idx, int x, int y);
static int get_variance(FILTER *fp, FILTER_PROC_INFO *fpip, int area_idx, int x, int y);
static short get_mean_y(FILTER_PROC_INFO *fpip, int area_idx, int x, int y);
static short get_mean_cb(FILTER_PROC_INFO *fpip, int area_idx, int x, int y);
static short get_mean_cr(FILTER_PROC_INFO *fpip, int area_idx, int x, int y);

//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define CHECK_N 2
static TCHAR check_name0[] = "注目点中心2次モーメントで領域を選択";
static TCHAR check_name1[] = "色差のみ平均化";
static TCHAR *check_name[CHECK_N] = {check_name0, check_name1};
static int check_default[CHECK_N]  = {0, 0};
#define FILTER_NAME "局所領域選択・平均化フィルタ"
static TCHAR filter_name[] = FILTER_NAME;
static TCHAR filter_information[] = FILTER_NAME " " VERSION " by KAZOON";

FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	0, 0, // 設定ウィンドウサイズ - 設定しない
	filter_name,
	0, // トラックバーの数
	NULL, NULL, NULL, NULL, // その他トラックバー関連
	CHECK_N,
	check_name,
	check_default,
	func_proc,
	NULL, // func_init
	NULL, // func_exit
	NULL, NULL, NULL, NULL, NULL, 0,
	filter_information,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	0, 0 // reserve
};

EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall
GetFilterTable(void)
{
	return &filter;
}

//---------------------------------------------------------------------
//		フィルタ処理関数
//---------------------------------------------------------------------
BOOL
func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	int i = -1;
	for (int y=0; y<fpip->h; y++) {
		if (y<=2 || y>=fpip->h-2) {
		} else {
			i -= 5;
		}
		for (int x=0; x<fpip->w; x++){
			if (x<=2 || x>=fpip->w-2) {
				i++;
			}
			set_smoothed(fp, fpip, i, x, y);
		}
	}

	PIXEL_YC *swap = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = swap;

	return TRUE;
}

//---------------------------------------------------------------------
//locals[local_idx]に基づき，(x, y)の周囲で最も輝度のばらつきが少ない領域を探査。
//見つかった領域での平均値をycp_tempに書き込む。
static void
set_smoothed(FILTER *fp, FILTER_PROC_INFO *fpip, int local_idx, int x, int y)
{
	int min = get_variance(fp, fpip, locals[local_idx][0], x, y);
	int min_idx = 0;
	for (int i=1; i<NI; i++) {
		int temp = locals[local_idx][i];
		if ( temp == NA ) {
			break;
		}
		temp = get_variance(fp, fpip, temp, x, y);
		if ( temp < min ) {
			min = temp;
			min_idx = i;
		}
	}
	min = locals[local_idx][min_idx];
	YCP_TEMP(fpip, x, y)->y = (fp->check[1] ? YCP_EDIT(fpip, x, y)->y : get_mean_y(fpip, min, x, y));
	YCP_TEMP(fpip, x, y)->cb = get_mean_cb(fpip, min, x, y);
	YCP_TEMP(fpip, x, y)->cr = get_mean_cr(fpip, min, x, y);
}

//(x, y)周り(areas[area_idx])の輝度の分散を返す。
static int
get_variance(FILTER *fp, FILTER_PROC_INFO *fpip, int area_idx, int x, int y)
{
	int mean = (fp->check[0] ? YCP_EDIT(fpip, x, y)->y : get_mean_y(fpip, area_idx, x, y));
	int sum = 0;
	int i;
	for (i=0; i<NI; i++) {
		int temp = areas[area_idx][i];
		if ( temp == NR ) {
			break;
		}
		temp = YCP_EDIT(fpip, x+rels[temp][0], y+rels[temp][1])->y - mean;
		sum += temp*temp;
	}
	
	if ( fp->check[0] ) {
		return sum/i;
	} else {
		return sum/(i-1);
	}
}

//(x, y)周り(areas[area_idx])の輝度の平均を返す。
static short
get_mean_y(FILTER_PROC_INFO *fpip, int area_idx, int x, int y)
{
	int sum = 0;
	int i;
	for (i=0; i<NI; i++) {
		int temp = areas[area_idx][i];
		if (temp==NR) {
			break;
		}
		sum += YCP_EDIT(fpip, x+rels[temp][0], y+rels[temp][1])->y;
	}

	return static_cast<short>(sum/i);
}

//(x, y)周り(areas[area_idx])の色差(青)の平均を返す。
static short
get_mean_cb(FILTER_PROC_INFO *fpip, int area_idx, int x, int y)
{
	int sum = 0;
	int i;
	for (i=0; i<NI; i++) {
		int temp = areas[area_idx][i];
		if (temp==NR) {
			break;
		}
		sum += YCP_EDIT(fpip, x+rels[temp][0], y+rels[temp][1])->cb;
	}

	return static_cast<short>(sum/i);
}
//(x, y)周り(areas[area_idx])の色差(赤)の平均を返す。
static short
get_mean_cr(FILTER_PROC_INFO *fpip, int area_idx, int x, int y)
{
	int sum = 0;
	int i;
	for (i=0; i<NI; i++) {
		int temp = areas[area_idx][i];
		if (temp==NR) {
			break;
		}
		sum += YCP_EDIT(fpip, x+rels[temp][0], y+rels[temp][1])->cr;
	}

	return static_cast<short>(sum/i);
}
