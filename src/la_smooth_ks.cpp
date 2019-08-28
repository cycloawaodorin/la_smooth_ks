//----------------------------------------------------------------------------------
//		局所領域選択・平均化フィルタ
//----------------------------------------------------------------------------------
#include <windows.h>
#include <stdlib.h>
#include <limits.h>
#include "filter.h"
#include "version.h"

#define RANGE 3
static int *mean_y[RANGE] = {nullptr, nullptr, nullptr};
static int *var_y[RANGE] = {nullptr, nullptr, nullptr};
static int *temp_y = nullptr;
static struct {
	int range, w;
} allocated = {0, 0};

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
	func_exit,
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

BOOL
func_exit(FILTER *fp)
{
	for ( int i=0; i<allocated.range; i++ ) {
		free(mean_y[i]);
		mean_y[i] = nullptr;
		free(var_y[i]);
		var_y[i] = nullptr;
		free(temp_y);
		temp_y = nullptr;
	}
	allocated.range = 0;
	allocated.w = 0;
	
	return TRUE;
}

//---------------------------------------------------------------------
//		フィルタ処理関数
//---------------------------------------------------------------------
#define YCP_EDIT(fpip, x, y) ((fpip)->ycp_edit+(x)+(fpip)->max_w*(y))
#define YCP_TEMP(fpip, x, y) ((fpip)->ycp_temp+(x)+(fpip)->max_w*(y))
#define PMOD(a, b) ( ((a)<(b)) ? (a) : ((a)-(b)) )

#define MID_Y 2048
#define IMIN (-1)
#define IMAX 2

#define NR 25
//相対座標リスト
static const int rels[NR][2] = {
	{-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2},
	{-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1},
	{-2, 0}, {-1, 0}, {0, 0}, {1, 0}, {2, 0},
	{-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1},
	{-2, 2}, {-1, 2}, {0, 2}, {1, 2}, {2, 2},
};
static const int * const exclusions[3][3][2] = {
	{{rels[2], rels[10]}, {rels[11], rels[13]}, {rels[2], rels[14]}},
	{{rels[7], rels[17]}, {nullptr, nullptr}, {rels[7], rels[17]}},
	{{rels[10], rels[22]}, {rels[11], rels[13]}, {rels[14], rels[22]}}
};

static void set_mean_var(const FILTER_PROC_INFO *fpip, int ii, int y);
static void set_smoothed(const FILTER *fp, FILTER_PROC_INFO *fpip, int x, int y, int offset);
static int get_var(const FILTER_PROC_INFO *fpip, int x, int y, int offset, int i, int j, BOOL offcentrize);
static short get_mean_y(const FILTER_PROC_INFO *fpip, int x, int y, int offset, int i, int j);
static short get_mean_cb(const FILTER_PROC_INFO *fpip, int x, int y, const int * const *list);
static short get_mean_cr(const FILTER_PROC_INFO *fpip, int x, int y, const int * const *list);

// 本体
BOOL
func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{
	if ( fpip->h < (RANGE*2-1) || fpip->w < (RANGE*2-1) ) {
		OutputDebugString("The image width or height is too small for the range.");
		return FALSE;
	}
	if ( allocated.w < fpip->w || allocated.range < RANGE ) {
		func_exit(fp);
		allocated.range = RANGE;
		allocated.w = fpip->w;
		for ( int i=0; i<RANGE; i++ ) {
			mean_y[i] = static_cast<int *>( malloc(sizeof(int)*static_cast<size_t>(fpip->w+RANGE)) );
			if ( mean_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
			var_y[i] = static_cast<int *>( malloc(sizeof(int)*static_cast<size_t>(fpip->w+RANGE)) );
			if ( var_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
		}
		temp_y = static_cast<int *>( malloc(sizeof(int)*fpip->w) );
		if ( temp_y == nullptr ) {
			OutputDebugString("Memory allocation failed.");
			return FALSE;
		}
	}
	
	int offset = -IMIN;
	for ( int i=IMIN; i<IMAX-1; i++ ) {
		set_mean_var(fpip, PMOD(i+offset, RANGE), i);
	}
	offset = 0;
	for ( int y=0; y<fpip->h; y++ ) {
		set_mean_var(fpip, PMOD(RANGE-1+offset, RANGE), y+IMAX-1);
		for ( int x=0; x<fpip->w; x++ ) {
			set_smoothed(fp, fpip, x, y, offset);
		}
		offset = PMOD(offset+1, RANGE);
	}

	PIXEL_YC *swap = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = swap;

	return TRUE;
}

// すべての x について，y を中心とした mean_y, var_y を i にセット
static void
set_mean_var(const FILTER_PROC_INFO *fpip, int ii, int y)
{
	// 輝度の平均
	int vskip = 0;
	for ( int x=0; x<fpip->w; x++ ) {
		temp_y[x] = 0;
		for ( int i=IMIN; i<IMAX; i++ ) {
			if ( y+i < 0 || fpip->h <= y+i ) {
				vskip++;
			} else {
				temp_y[x] += YCP_EDIT(fpip, x, y+i)->y;
			}
		}
	}
	for ( int x=IMIN; x<fpip->w+IMAX; x++ ) {
		int xx = x-IMIN;
		mean_y[ii][xx] = 0;
		int hskip = 0;
		for ( int j=IMIN; j<IMAX; j++ ) {
			if ( x+j < 0 || fpip->w <= x+j ) {
				hskip++;
			} else {
				mean_y[ii][xx] += temp_y[x+j];
			}
		}
		if ( (RANGE-vskip)*(RANGE-hskip) != 0 ) {
			mean_y[ii][xx] /= (RANGE-vskip)*(RANGE-hskip);
		}
	}
	
	// 輝度の分散
	for ( int x=0; x<fpip->w; x++ ) {
		temp_y[x] = 0;
		for ( int i=IMIN; i<IMAX; i++ ) {
			if ( y+i < 0 ) {
				continue;
			} else if ( fpip->w <= y+i ) {
				break;
			} else {
				int temp = YCP_EDIT(fpip, x, y+i)->y - MID_Y;
				temp_y[x] += temp*temp;
			}
		}
	}
	for ( int x=IMIN; x<fpip->w+IMAX; x++ ) {
		int xx = x-IMIN;
		var_y[ii][xx] = 0;
		int hskip = 0;
		for ( int j=IMIN; j<IMAX; j++ ) {
			if ( x+j < 0 || fpip->w <= x+j ) {
				hskip++;
			}  else {
				var_y[ii][xx] += temp_y[x+j];
			}
		}
		int den = (RANGE-vskip)*(RANGE-hskip);
		if ( den < 2 ) {
			var_y[ii][xx] = INT_MAX;
		} else {
			var_y[ii][xx] /= den-1;
		}
		int temp = mean_y[ii][xx]-MID_Y;
		var_y[ii][xx] -= temp*temp*den/(den-1);
	}
}

// x, y の平滑化画素値を ycp_temp にセット
static void
set_smoothed(const FILTER *fp, FILTER_PROC_INFO *fpip, int x, int y, int offset)
{
	int min_var = get_var(fpip, x, y, offset, 0, 0, fp->check[0]);
	int min_i=0, min_j=0;
	for ( int i=0; i<RANGE; i++ ) {
		for ( int j=(!i); j<RANGE; j++ ) {
			int var = get_var(fpip, x, y, offset, i, j, fp->check[0]);
			if ( var < min_var ) {
				min_var = var;
				min_i = i;
				min_j = j;
			}
		}
	}
	YCP_TEMP(fpip, x, y)->y = fp->check[1] ? YCP_EDIT(fpip, x, y)-> y : get_mean_y(fpip, x, y, offset, min_i, min_j);
	YCP_TEMP(fpip, x, y)->cb = get_mean_cb(fpip, x+min_i+IMIN, y+min_j+IMIN, exclusions[min_i][min_j]);
	YCP_TEMP(fpip, x, y)->cr = get_mean_cr(fpip, x+min_i+IMIN, y+min_j+IMIN, exclusions[min_i][min_j]);
}

static int
get_var(const FILTER_PROC_INFO *fpip, int x, int y, int offset, int i, int j, BOOL offcentrize)
{
	int ii = PMOD(i+offset, RANGE);
	const int * const *list = exclusions[i][j];
	int ret = var_y[ii][x+j];
	int diff=0, d_m=0, d_cnt=0;
	
	for ( int k=0; k<2; k++ ) {
		if ( list[k] != nullptr ) {
			int xx = x + list[k][0];
			int yy = y + list[k][1];
			if ( 0 <= xx && xx < fpip->w && 0 <= yy && yy < fpip->h ) {
				int temp = mean_y[ii][x+j] - YCP_EDIT(fpip, xx, yy)->y;
				diff += temp*temp;
				d_cnt += 1;
				d_m += temp;
			}
		}
	}
	if ( 0 < d_cnt ) {
		// 本当は 9 じゃなくて参照した画素数にしなきゃいけないけど，
		// 9 じゃないのは端っこだけだし，面倒なので細かいところは無視する
		int temp = d_m/(9-d_cnt);
		ret = (8*ret-diff)/(8-d_cnt)-temp*temp;
	}
	if ( offcentrize ) {
		int temp = mean_y[ii][x+j] - YCP_EDIT(fpip, x, y)->y;
		ret += temp*temp;
	}
	
	return ret;
}

static short
get_mean_y(const FILTER_PROC_INFO *fpip, int x, int y, int offset, int i, int j)
{
	int ii = PMOD(i+offset, RANGE);
	const int * const *list = exclusions[i][j];
	int ret = mean_y[ii][x+j];
	int d_m=0, d_cnt=0;
	
	for ( int k=0; k<2; k++ ) {
		if ( list[k] != nullptr ) {
			int xx = x + list[k][0];
			int yy = y + list[k][1];
			if ( 0 <= xx && xx < fpip->w && 0 <= yy && yy < fpip->h ) {
				d_cnt += 1;
				d_m += YCP_EDIT(fpip, xx, yy)->y;
			}
		}
	}
	if ( 0 < d_cnt ) {
		// 本当は 9 じゃなくて参照した画素数にしなきゃいけないけど，
		// 9 じゃないのは端っこだけだし，面倒なので細かいところは無視する
		ret = (9*ret-d_m)/(9-d_cnt);
	}
	
	return static_cast<short>( ret );
}

static short
get_mean_cb(const FILTER_PROC_INFO *fpip, int x, int y, const int * const *list)
{
	int sum = 0, vskip = 0, hskip=0;
	for ( int i=IMIN; i<IMAX; i++ ) {
		int yy = y+i;
		if ( yy < 0 || fpip->h <= yy ) {
			vskip++;
			continue;
		}
		for ( int j=IMIN; j<IMAX; j++ ) {
			int xx = x+j;
			if ( xx < 0 || fpip->w <= xx || ( list[0] != nullptr && ( (list[0][0] == i && list[0][1] == j) || (list[1][0] == i && list[1][1] == j) )) ) {
				hskip++;
				continue;
			}
			sum += YCP_EDIT(fpip, xx, yy)->cb;
		}
	}
	
	return static_cast<short>( sum / ( (RANGE-vskip)*RANGE-hskip ) );
}

static short
get_mean_cr(const FILTER_PROC_INFO *fpip, int x, int y, const int * const *list)
{
	int sum = 0, vskip = 0, hskip=0;
	for ( int i=IMIN; i<IMAX; i++ ) {
		int yy = y+i;
		if ( yy < 0 || fpip->h <= yy ) {
			vskip++;
			continue;
		}
		for ( int j=IMIN; j<IMAX; j++ ) {
			int xx = x+j;
			if ( xx < 0 || fpip->w <= xx || ( list[0] != nullptr && ( (list[0][0] == i && list[0][1] == j) || (list[1][0] == i && list[1][1] == j) )) ) {
				hskip++;
				continue;
			}
			sum += YCP_EDIT(fpip, xx, yy)->cr;
		}
	}
	
	return static_cast<short>( sum / ( (RANGE-vskip)*RANGE-hskip ) );
}
