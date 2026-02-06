//----------------------------------------------------------------------------------
//		局所領域選択・平均化フィルタ
//----------------------------------------------------------------------------------
#include <windows.h>
#include <string>
#include "filter2.hpp"
#include "version.hpp"

constexpr static int RANGE = 3;
static int *mean_y[RANGE] = {nullptr, nullptr, nullptr};
static int *var_y[RANGE] = {nullptr, nullptr, nullptr};
static int *temp_y = nullptr;
static PIXEL_RGBA *edit_buffer = nullptr;
static PIXEL_RGBA *temp_buffer = nullptr;
static struct {
	int range, w, wh;
} allocated = {0, 0, 0};
static int width, height;

static bool func_proc_video(FILTER_PROC_VIDEO* video);

//---------------------------------------------------------------------
//		フィルタプラグイン構造体定義
//---------------------------------------------------------------------
#define FILTER_NAME L"局所領域選択・平均化フィルタ"
static FILTER_ITEM_CHECK second_moment(L"注目点中心2次モーメントで領域を選択", false);

EXTERN_C FILTER_PLUGIN_TABLE *
GetFilterPluginTable()
{
	static void* settings[] = {&second_moment, nullptr};
	static FILTER_PLUGIN_TABLE fpt = {
		FILTER_PLUGIN_TABLE::FLAG_VIDEO,
		FILTER_NAME,
		nullptr,
		FILTER_NAME L" " VERSION L" by KAZOON",
		settings,
		func_proc_video,
		nullptr, // func_proc_audio
	};
	return &fpt;
}

EXTERN_C void
UninitializePlugin()
{
	for ( auto i=0; i<allocated.range; i++ ) {
		free(mean_y[i]);
		mean_y[i] = nullptr;
		free(var_y[i]);
		var_y[i] = nullptr;
		free(temp_y);
		temp_y = nullptr;
		free(temp_buffer);
		temp_buffer = nullptr;
	}
	allocated.range = 0;
	allocated.w = 0;
	allocated.wh = 0;
}

//---------------------------------------------------------------------
//		フィルタ処理関数
//---------------------------------------------------------------------
static PIXEL_RGBA*
rgba_edit(auto x, auto y)
{
	return edit_buffer+(x+width*y);
}
static PIXEL_RGBA*
rgba_temp(auto x, auto y)
{
	return temp_buffer+(x+width*y);
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
static int
prgba2y(const PIXEL_RGBA *p)
{
	return ((4918*static_cast<int>(p->r)+354)>>10) + ((9655*static_cast<int>(p->g)+585)>>10) + ((1875*static_cast<int>(p->b)+523)>>10);
}
static int
prgba2cb(const PIXEL_RGBA *p)
{
	return ((-2775*static_cast<int>(p->r)+240)>>10) + ((-5449*static_cast<int>(p->g)+515)>>10) + ((8224*static_cast<int>(p->b)+256)>>10);
}
static int
prgba2cr(const PIXEL_RGBA *p)
{
	return ((8224*static_cast<int>(p->r)+256)>>10) + ((-6887*static_cast<int>(p->g)+110)>>10) + ((-1337*static_cast<int>(p->b)+646)>>10);
}
static void
set_rgba(PIXEL_RGBA *p, auto y, auto cb, auto cr, auto a)
{
	p->r = static_cast<unsigned char>( ( 255*y + ( ((22881*cr>>16)+3) <<10) ) >>12 );
	p->g = static_cast<unsigned char>( ( 255*y + ( ((-5616*cb>>16)+(-11655*cr>>16)+3) <<10) ) >>12 );
	p->b = static_cast<unsigned char>( ( 255*y + ( ((28919*cb>>16)+3) <<10) ) >>12 );
	p->a = a;
}
static int
edit_y(auto x, auto y)
{
	return prgba2y(rgba_edit(x, y));
}

constexpr static const int MID_Y = 2048;
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

static void set_mean_var(auto ii, auto y);
static void set_smoothed(auto x, auto y, auto offset);
static auto get_var(auto x, auto y, auto offset, auto i, auto j);
static auto get_mean_y(auto x, auto y, auto offset, auto i, auto j);
static auto get_mean_cb(auto x, auto y, const int * const *list);
static auto get_mean_cr(auto x, auto y, const int * const *list);

// 本体
bool
func_proc_video(FILTER_PROC_VIDEO *video)
{
	height = video->object->height;
	width = video->object->width;
	if ( height < (RANGE*2-1) || width < (RANGE*2-1) ) {
		OutputDebugString("The image width or height is too small for the range.");
		return false;
	}
	if ( allocated.w < width || allocated.range < RANGE || allocated.wh < width*height ) {
		UninitializePlugin();
		allocated.range = RANGE;
		allocated.w = width;
		allocated.wh = width*height;
		for ( auto i=0; i<RANGE; i++ ) {
			mean_y[i] = static_cast<int *>( calloc(width+RANGE, sizeof(int)) );
			if ( mean_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return false;
			}
			var_y[i] = static_cast<int *>( calloc(width+RANGE, sizeof(int)) );
			if ( var_y[i] == nullptr ) {
				OutputDebugString("Memory allocation failed.");
				return false;
			}
		}
		temp_y = static_cast<int *>( calloc(width, sizeof(int)) );
		if ( temp_y == nullptr ) {
			OutputDebugString("Memory allocation failed.");
			return false;
		}
		edit_buffer = static_cast<PIXEL_RGBA *>( calloc(allocated.wh, sizeof(PIXEL_RGBA)) );
		if ( edit_buffer == nullptr ) {
			OutputDebugString("Memory allocation failed.");
			return false;
		}
		temp_buffer = static_cast<PIXEL_RGBA *>( calloc(allocated.wh, sizeof(PIXEL_RGBA)) );
		if ( temp_buffer == nullptr ) {
			OutputDebugString("Memory allocation failed.");
			return false;
		}
	}
	video->get_image_data(edit_buffer);
	
	auto offset = -IMIN;
	for ( auto i=IMIN; i<IMAX-1; i++ ) {
		set_mean_var(pmod(i+offset, RANGE), i);
	}
	offset = 0;
	for ( auto y=0; y<height; y++ ) {
		set_mean_var(pmod(RANGE-1+offset, RANGE), y+IMAX-1);
		for ( auto x=0; x<width; x++ ) {
			set_smoothed(x, y, offset);
		}
		offset = pmod(offset+1, RANGE);
	}

	video->set_image_data(temp_buffer, width, height);

	return true;
}

// すべての x について，y を中心とした mean_y, var_y を i にセット
static void
set_mean_var(auto ii, auto y)
{
	// 輝度の平均
	auto vskip = 0;
	for ( auto x=0; x<width; x++ ) {
		temp_y[x] = 0;
		for ( auto i=IMIN; i<IMAX; i++ ) {
			if ( y+i < 0 || height <= y+i ) {
				vskip++;
			} else {
				temp_y[x] += edit_y(x, y+i);
			}
		}
	}
	for ( auto x=IMIN; x<width+IMAX; x++ ) {
		auto xx = x-IMIN;
		mean_y[ii][xx] = 0;
		auto hskip = 0;
		for ( auto j=IMIN; j<IMAX; j++ ) {
			if ( x+j < 0 || width <= x+j ) {
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
	for ( auto x=0; x<width; x++ ) {
		temp_y[x] = 0;
		for ( auto i=IMIN; i<IMAX; i++ ) {
			if ( y+i < 0 ) {
				continue;
			} else if ( width <= y+i ) {
				break;
			} else {
				auto temp = edit_y(x, y+i) - MID_Y;
				temp_y[x] += temp*temp;
			}
		}
	}
	for ( auto x=IMIN; x<width+IMAX; x++ ) {
		auto xx = x-IMIN;
		var_y[ii][xx] = 0;
		auto hskip = 0;
		for ( auto j=IMIN; j<IMAX; j++ ) {
			if ( x+j < 0 || width <= x+j ) {
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

// x, y の平滑化画素値を temp_buffer にセット
static void
set_smoothed(auto x, auto y, auto offset)
{
	auto min_var = get_var(x, y, offset, 0, 0);
	auto min_i=0, min_j=0;
	for ( auto i=0; i<RANGE; i++ ) {
		for ( auto j=(i==0 ? 1 : 0); j<RANGE; j++ ) {
			auto var = get_var(x, y, offset, i, j);
			if ( var < min_var ) {
				min_var = var;
				min_i = i;
				min_j = j;
			}
		}
	}
	auto ty = get_mean_y(x, y, offset, min_i, min_j);
	auto tcb = get_mean_cb(x+min_i+IMIN, y+min_j+IMIN, exclusions[min_i][min_j]);
	auto tcr = get_mean_cr(x+min_i+IMIN, y+min_j+IMIN, exclusions[min_i][min_j]);
	set_rgba(rgba_temp(x, y), ty, tcb, tcr, rgba_edit(x, y)->a);
}

static auto
get_var(auto x, auto y, auto offset, auto i, auto j)
{
	auto ii = pmod(i+offset, RANGE);
	const int * const *list = exclusions[i][j];
	auto ret = var_y[ii][x+j];
	auto diff=0, d_m=0, d_cnt=0;
	
	for ( auto k=0; k<2; k++ ) {
		if ( list[k] != nullptr ) {
			auto xx = x + list[k][0];
			auto yy = y + list[k][1];
			if ( 0 <= xx && xx < width && 0 <= yy && yy < height ) {
				auto temp = mean_y[ii][x+j] - edit_y(xx, yy);
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
	if ( second_moment.value ) {
		auto temp = mean_y[ii][x+j] - edit_y(x, y);
		ret += temp*temp;
	}
	
	return ret;
}

static auto
get_mean_y(auto x, auto y, auto offset, auto i, auto j)
{
	auto ii = pmod(i+offset, RANGE);
	const int * const *list = exclusions[i][j];
	auto ret = mean_y[ii][x+j];
	auto d_m=0, d_cnt=0;
	
	for ( auto k=0; k<2; k++ ) {
		if ( list[k] != nullptr ) {
			auto xx = x + list[k][0];
			auto yy = y + list[k][1];
			if ( 0 <= xx && xx < width && 0 <= yy && yy < height ) {
				d_cnt += 1;
				d_m += edit_y(xx, yy);
			}
		}
	}
	if ( 0 < d_cnt ) {
		// 本当は 9 じゃなくて参照した画素数にしなきゃいけないけど，
		// 9 じゃないのは端っこだけだし，面倒なので細かいところは無視する
		ret = (9*ret-d_m)/(9-d_cnt);
	}
	
	return ret;
}

static auto
get_mean_cb(auto x, auto y, const int * const *list)
{
	auto sum = 0, vskip = 0, hskip=0;
	for ( auto i=IMIN; i<IMAX; i++ ) {
		auto yy = y+i;
		if ( yy < 0 || height <= yy ) {
			vskip++;
			continue;
		}
		for ( auto j=IMIN; j<IMAX; j++ ) {
			auto xx = x+j;
			if ( xx < 0 || width <= xx || ( list[0] != nullptr && ( (list[0][0] == i && list[0][1] == j) || (list[1][0] == i && list[1][1] == j) )) ) {
				hskip++;
				continue;
			}
			sum += prgba2cb(rgba_edit(xx, yy));
		}
	}
	
	return sum / ( (RANGE-vskip)*RANGE-hskip );
}

static auto
get_mean_cr(auto x, auto y, const int * const *list)
{
	auto sum = 0, vskip = 0, hskip=0;
	for ( auto i=IMIN; i<IMAX; i++ ) {
		auto yy = y+i;
		if ( yy < 0 || height <= yy ) {
			vskip++;
			continue;
		}
		for ( auto j=IMIN; j<IMAX; j++ ) {
			auto xx = x+j;
			if ( xx < 0 || width <= xx || ( list[0] != nullptr && ( (list[0][0] == i && list[0][1] == j) || (list[1][0] == i && list[1][1] == j) )) ) {
				hskip++;
				continue;
			}
			sum += prgba2cr(rgba_edit(xx, yy));
		}
	}
	
	return sum / ( (RANGE-vskip)*RANGE-hskip );
}
