//----------------------------------------------------------------------------------
//		局所領域選択・平均化フィルタ
//----------------------------------------------------------------------------------
#include <windows.h>
#include <string>
#include "filter.hpp"
#include "version.hpp"

constexpr static int RANGE = 3;
static int *mean_y[RANGE] = {nullptr, nullptr, nullptr};
static int *var_y[RANGE] = {nullptr, nullptr, nullptr};
static int *temp_y = nullptr;
static struct {
	int range, w;
} allocated = {0, 0};

static std::string
Utf16ToCp932(const std::wstring &utf16)
{
	if (utf16.empty()) return "";
	auto size = WideCharToMultiByte(932, 0, utf16.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string str(size - 1, '\0');
	WideCharToMultiByte(932, 0, utf16.c_str(), -1, str.data(), size, nullptr, nullptr);
	return str;
}

//---------------------------------------------------------------------
//		フィルタ構造体定義
//---------------------------------------------------------------------
#define FILTER_NAME L"局所領域選択・平均化フィルタ"

EXTERN_C FILTER_DLL *
GetFilterTable()
{
	constexpr static const auto CHECK_N = 2;
	static const std::wstring check_name0 = L"注目点中心2次モーメントで領域を選択";
	static const std::wstring check_name1 = L"色差のみ平均化";
	static TCHAR *check_name[CHECK_N] = {const_cast<TCHAR *>(Utf16ToCp932(check_name0).c_str()), const_cast<TCHAR *>(Utf16ToCp932(check_name1).c_str())};
	static int check_default[CHECK_N]  = {0, 0};
	static FILTER_DLL filter = {
		FILTER_FLAG_EX_INFORMATION,
		0, 0, // 設定ウィンドウサイズ - 設定しない
		const_cast<TCHAR *>(Utf16ToCp932(FILTER_NAME).c_str()),
		0, // トラックバーの数
		nullptr, nullptr, nullptr, nullptr, // その他トラックバー関連
		CHECK_N,
		check_name,
		check_default,
		func_proc,
		nullptr, // func_init
		func_exit,
		nullptr, nullptr, nullptr, nullptr, nullptr, 0,
		const_cast<TCHAR *>(Utf16ToCp932(FILTER_NAME L" " VERSION L" by KAZOON").c_str()),
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
		0, 0 // reserve
	};
	return &filter;
}

BOOL
func_exit(FILTER *fp)
{
	for ( auto i=0; i<allocated.range; i++ ) {
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
static PIXEL_YC*
ycp_edit(const FILTER_PROC_INFO *fpip, auto x, auto y)
{
	return fpip->ycp_edit+(x+fpip->max_w*y);
}
static PIXEL_YC*
ycp_temp(const FILTER_PROC_INFO *fpip, auto x, auto y)
{
	return fpip->ycp_temp+(x+fpip->max_w*y);
}
static auto
pmod(auto a, auto b)
{
	if ( a < b ) {
		return a;
	} else {
		return a-b;
	}
}

constexpr static const short MID_Y = 2048;
constexpr static const int IMIN = -1;
constexpr static const int IMAX = 2;
constexpr static const int NR = 25;
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

static void set_mean_var(const FILTER_PROC_INFO *fpip, auto ii, auto y);
static void set_smoothed(const FILTER *fp, FILTER_PROC_INFO *fpip, auto x, auto y, auto offset);
static auto get_var(const FILTER_PROC_INFO *fpip, auto x, auto y, auto offset, auto i, auto j, auto offcentrize);
static short get_mean_y(const FILTER_PROC_INFO *fpip, auto x, auto y, auto offset, auto i, auto j);
static short get_mean_cb(const FILTER_PROC_INFO *fpip, auto x, auto y, const int * const *list);
static short get_mean_cr(const FILTER_PROC_INFO *fpip, auto x, auto y, const int * const *list);

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
		for ( auto i=0; i<RANGE; i++ ) {
			mean_y[i] = static_cast<int *>( calloc(fpip->w+RANGE, sizeof(int)) );
			if ( mean_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
			var_y[i] = static_cast<int *>( calloc(fpip->w+RANGE, sizeof(int)) );
			if ( var_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return FALSE;
			}
		}
		temp_y = static_cast<int *>( calloc(fpip->w, sizeof(int)) );
		if ( temp_y == nullptr ) {
			OutputDebugString("Memory allocation failed.");
			return FALSE;
		}
	}
	
	auto offset = -IMIN;
	for ( auto i=IMIN; i<IMAX-1; i++ ) {
		set_mean_var(fpip, pmod(i+offset, RANGE), i);
	}
	offset = 0;
	for ( auto y=0; y<fpip->h; y++ ) {
		set_mean_var(fpip, pmod(RANGE-1+offset, RANGE), y+IMAX-1);
		for ( auto x=0; x<fpip->w; x++ ) {
			set_smoothed(fp, fpip, x, y, offset);
		}
		offset = pmod(offset+1, RANGE);
	}

	PIXEL_YC *swap = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = swap;

	return TRUE;
}

// すべての x について，y を中心とした mean_y, var_y を i にセット
static void
set_mean_var(const FILTER_PROC_INFO *fpip, auto ii, auto y)
{
	// 輝度の平均
	auto vskip = 0;
	for ( auto x=0; x<fpip->w; x++ ) {
		temp_y[x] = 0;
		for ( auto i=IMIN; i<IMAX; i++ ) {
			if ( y+i < 0 || fpip->h <= y+i ) {
				vskip++;
			} else {
				temp_y[x] += ycp_edit(fpip, x, y+i)->y;
			}
		}
	}
	for ( auto x=IMIN; x<fpip->w+IMAX; x++ ) {
		auto xx = x-IMIN;
		mean_y[ii][xx] = 0;
		auto hskip = 0;
		for ( auto j=IMIN; j<IMAX; j++ ) {
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
	for ( auto x=0; x<fpip->w; x++ ) {
		temp_y[x] = 0;
		for ( auto i=IMIN; i<IMAX; i++ ) {
			if ( y+i < 0 ) {
				continue;
			} else if ( fpip->w <= y+i ) {
				break;
			} else {
				auto temp = ycp_edit(fpip, x, y+i)->y - MID_Y;
				temp_y[x] += temp*temp;
			}
		}
	}
	for ( auto x=IMIN; x<fpip->w+IMAX; x++ ) {
		auto xx = x-IMIN;
		var_y[ii][xx] = 0;
		auto hskip = 0;
		for ( auto j=IMIN; j<IMAX; j++ ) {
			if ( x+j < 0 || fpip->w <= x+j ) {
				hskip++;
			}  else {
				var_y[ii][xx] += temp_y[x+j];
			}
		}
		auto den = (RANGE-vskip)*(RANGE-hskip);
		if ( den < 2 ) {
			var_y[ii][xx] = INT_MAX;
		} else {
			var_y[ii][xx] /= den-1;
		}
		auto temp = mean_y[ii][xx]-MID_Y;
		var_y[ii][xx] -= temp*temp*den/(den-1);
	}
}

// x, y の平滑化画素値を ycp_temp にセット
static void
set_smoothed(const FILTER *fp, FILTER_PROC_INFO *fpip, auto x, auto y, auto offset)
{
	auto min_var = get_var(fpip, x, y, offset, 0, 0, fp->check[0]);
	auto min_i=0, min_j=0;
	for ( auto i=0; i<RANGE; i++ ) {
		for ( auto j=(i==0 ? 1 : 0); j<RANGE; j++ ) {
			auto var = get_var(fpip, x, y, offset, i, j, fp->check[0]);
			if ( var < min_var ) {
				min_var = var;
				min_i = i;
				min_j = j;
			}
		}
	}
	ycp_temp(fpip, x, y)->y = fp->check[1] ? ycp_edit(fpip, x, y)-> y : get_mean_y(fpip, x, y, offset, min_i, min_j);
	ycp_temp(fpip, x, y)->cb = get_mean_cb(fpip, x+min_i+IMIN, y+min_j+IMIN, exclusions[min_i][min_j]);
	ycp_temp(fpip, x, y)->cr = get_mean_cr(fpip, x+min_i+IMIN, y+min_j+IMIN, exclusions[min_i][min_j]);
}

static auto
get_var(const FILTER_PROC_INFO *fpip, auto x, auto y, auto offset, auto i, auto j, auto offcentrize)
{
	auto ii = pmod(i+offset, RANGE);
	const int * const *list = exclusions[i][j];
	auto ret = var_y[ii][x+j];
	auto diff=0, d_m=0, d_cnt=0;
	
	for ( auto k=0; k<2; k++ ) {
		if ( list[k] != nullptr ) {
			auto xx = x + list[k][0];
			auto yy = y + list[k][1];
			if ( 0 <= xx && xx < fpip->w && 0 <= yy && yy < fpip->h ) {
				auto temp = mean_y[ii][x+j] - ycp_edit(fpip, xx, yy)->y;
				diff += temp*temp;
				d_cnt += 1;
				d_m += temp;
			}
		}
	}
	if ( 0 < d_cnt ) {
		// 本当は 9 じゃなくて参照した画素数にしなきゃいけないけど，
		// 9 じゃないのは端っこだけだし，面倒なので細かいところは無視する
		auto temp = d_m/(9-d_cnt);
		ret = (8*ret-diff)/(8-d_cnt)-temp*temp;
	}
	if ( offcentrize ) {
		auto temp = mean_y[ii][x+j] - ycp_edit(fpip, x, y)->y;
		ret += temp*temp;
	}
	
	return ret;
}

static short
get_mean_y(const FILTER_PROC_INFO *fpip, auto x, auto y, auto offset, auto i, auto j)
{
	auto ii = pmod(i+offset, RANGE);
	const int * const *list = exclusions[i][j];
	auto ret = mean_y[ii][x+j];
	auto d_m=0, d_cnt=0;
	
	for ( auto k=0; k<2; k++ ) {
		if ( list[k] != nullptr ) {
			auto xx = x + list[k][0];
			auto yy = y + list[k][1];
			if ( 0 <= xx && xx < fpip->w && 0 <= yy && yy < fpip->h ) {
				d_cnt += 1;
				d_m += ycp_edit(fpip, xx, yy)->y;
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
get_mean_cb(const FILTER_PROC_INFO *fpip, auto x, auto y, const int * const *list)
{
	auto sum = 0, vskip = 0, hskip=0;
	for ( auto i=IMIN; i<IMAX; i++ ) {
		auto yy = y+i;
		if ( yy < 0 || fpip->h <= yy ) {
			vskip++;
			continue;
		}
		for ( auto j=IMIN; j<IMAX; j++ ) {
			auto xx = x+j;
			if ( xx < 0 || fpip->w <= xx || ( list[0] != nullptr && ( (list[0][0] == i && list[0][1] == j) || (list[1][0] == i && list[1][1] == j) )) ) {
				hskip++;
				continue;
			}
			sum += ycp_edit(fpip, xx, yy)->cb;
		}
	}
	
	return static_cast<short>( sum / ( (RANGE-vskip)*RANGE-hskip ) );
}

static short
get_mean_cr(const FILTER_PROC_INFO *fpip, auto x, auto y, const int * const *list)
{
	auto sum = 0, vskip = 0, hskip=0;
	for ( auto i=IMIN; i<IMAX; i++ ) {
		auto yy = y+i;
		if ( yy < 0 || fpip->h <= yy ) {
			vskip++;
			continue;
		}
		for ( auto j=IMIN; j<IMAX; j++ ) {
			auto xx = x+j;
			if ( xx < 0 || fpip->w <= xx || ( list[0] != nullptr && ( (list[0][0] == i && list[0][1] == j) || (list[1][0] == i && list[1][1] == j) )) ) {
				hskip++;
				continue;
			}
			sum += ycp_edit(fpip, xx, yy)->cr;
		}
	}
	
	return static_cast<short>( sum / ( (RANGE-vskip)*RANGE-hskip ) );
}
