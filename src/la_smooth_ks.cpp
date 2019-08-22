//----------------------------------------------------------------------------------
//		局所領域選択・平均化フィルタ
//----------------------------------------------------------------------------------
#include <windows.h>
#include <stdlib.h>
#include <float.h>
#include "filter.h"
#include "version.h"

#define RANGE 3
static float *mean_y[RANGE] = {nullptr, nullptr, nullptr};
static float *var_y[RANGE] = {nullptr, nullptr, nullptr};
static float *temp_y = nullptr;
static struct {
	int range, w;
} allocated = {0, 0};

#include <stdio.h>
#define DebugPrint( fmt, ... ) \
{ \
char str[256]; \
sprintf( str, fmt, __VA_ARGS__ ); \
OutputDebugString( str ); \
}


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
#define PMOD(a, b) ( (a)%(b)<0 ? (a)%(b)+(b) : (a)%(b) )

#define MID_Y 2048.0f
static int imin, imax;

static void set_mean_var(FILTER_PROC_INFO *fpip, int ii, int y);
static void set_smoothed(FILTER_PROC_INFO *fpip, int x, int y, int offset);

// 本体
BOOL
func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
{

	if ( fpip->h < (RANGE*2-1) || fpip->w < (RANGE*2-1) ) {
		OutputDebugString("The image width or height is too small for the range.");
		return FALSE;
	}
	if ( allocated.range < RANGE || allocated.w < fpip->w ) {
		func_exit(fp);
		allocated.range = RANGE;
		allocated.w = fpip->w;
		for ( int i=0; i<RANGE; i++ ) {
			mean_y[i] = static_cast<float *>( malloc(sizeof(float)*static_cast<size_t>(fpip->w+RANGE)) );
			if ( mean_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
			var_y[i] = static_cast<float *>( malloc(sizeof(float)*static_cast<size_t>(fpip->w+RANGE)) );
			if ( var_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
		}
		temp_y = static_cast<float *>( malloc(sizeof(float)*fpip->w) );
		if ( temp_y == nullptr ) {
			OutputDebugString("Memory allocation failed.");
			return FALSE;
		}
	}
	
	imin = -(RANGE/2);
	imax = imin + RANGE;
	int offset = -imin;
	for ( int i=imin; i<imax-1; i++ ) {
		set_mean_var(fpip, PMOD(i+offset, RANGE), i);
	}
	for ( int y=0; y<fpip->h; y++ ) {
		int i=imax-1;
		set_mean_var(fpip, PMOD(i+offset, RANGE), y+i);
		for ( int x=0; x<fpip->w; x++ ) {
			set_smoothed(fpip, x, y, offset);
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
set_mean_var(FILTER_PROC_INFO *fpip, int ii, int y)
{
	// 輝度の平均
	int vskip = 0;
	for ( int x=0; x<fpip->w; x++ ) {
		temp_y[x] = 0;
		for ( int i=imin; i<imax; i++ ) {
			if ( y+i < 0 || fpip->h <= y+i ) {
				vskip++;
			} else {
				temp_y[x] += YCP_EDIT(fpip, x, y+i)->y;
			}
		}
	}
	for ( int x=imin; x<fpip->w+imax; x++ ) {
		int xx = x-imin;
		mean_y[ii][xx] = 0;
		int hskip = 0;
		for ( int j=imin; j<imax; j++ ) {
			if ( x+j < 0 || fpip->w <= x+j ) {
				hskip++;
			} else {
				mean_y[ii][xx] += temp_y[x+j];
			}
		}
		mean_y[ii][xx] /= static_cast<float>( (RANGE-vskip)*(RANGE-hskip) );
	}
	
	// 輝度の分散
	for ( int x=0; x<fpip->w; x++ ) {
		temp_y[x] = 0;
		for ( int i=imin; i<imax; i++ ) {
			if ( y+i < 0 ) {
				continue;
			} else if ( fpip->w <= y+i ) {
				break;
			} else {
				float temp = YCP_EDIT(fpip, x, y+i)->y - MID_Y;
				temp_y[x] += temp*temp;
			}
		}
	}
	for ( int x=imin; x<fpip->w+imax; x++ ) {
		int xx = x-imin;
		var_y[ii][xx] = 0;
		int hskip = 0;
		for ( int j=imin; j<imax; j++ ) {
			if ( x+j < 0 || fpip->h <= x+j ) {
				hskip++;
			}  else {
				var_y[ii][xx] += temp_y[x+j];
			}
		}
		float den = static_cast<float>( (RANGE-vskip)*(RANGE-hskip) );
		if ( den < 1.5f ) {
			var_y[ii][xx] = FLT_MAX;
		} else {
			var_y[ii][xx] /= den-1.0f;
		}
		float temp = mean_y[ii][xx]-MID_Y;
		var_y[ii][xx] -= temp*temp*den/(den-1.0f);
	}
}

// x, y の平滑化画素値を ycp_temp にセット
static void
set_smoothed(FILTER_PROC_INFO *fpip, int x, int y, int offset)
{
	float min_var = var_y[offset][x];
	int min_i=0, min_j=0;
	for ( int i=0; i<RANGE; i++ ) {
		int ii = PMOD(i+offset, RANGE);
		for ( int j=(!i); j<RANGE; j++ ) {
			if ( var_y[ii][x+i] < min_var ) {
				min_var = var_y[ii][x+j];
				min_i = ii;
				min_j = j;
			}
		}
	}
	YCP_TEMP(fpip, x, y)->y = static_cast<short>( mean_y[min_i][x+min_j] );
}
