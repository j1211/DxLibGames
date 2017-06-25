//マップ作成
//・マウス左ドラッグで{線分(障害物), 長方形(盤面), ボール(自分), 地面(ゴール), アイテム(ゴールを流れるアイテム)}を引く. 
//・マウス右クリックでセーブ.
//・Zキークリックで一つ前の状態に戻す.
//
//(map_data.txt), 書式 (単位は基本的にメートル, 実数)
//PixelPerMeter 1メートルが何ピクセルに相当するか？  #meter -> pixelの変換で使う, なおデフォルトでは(x, y) = (0[meter], 0[meter])が左上と対応している.
//Gravity 重力加速度（m/s^2）
//Ball ボールの{中心}, {半径}
//Rect 長方形の{左上}, {右下} (x->y)
//Line 線分の個数
//・線分の{始点}, {終点} (x->y)
//Goal ゴールの{y座標}
//GoalItem {ゴールアイテムの個数}, {ゴールアイテムの(循環)移動スピード}
//・ゴールアイテムの{中心}, {半径}

#include "DxLib.h"
#include <complex>
#include <stack>
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <iomanip>
using namespace std;

typedef complex<double> Point;

const int WINDOW_SIZE_X = 800;
const int WINDOW_SIZE_Y = 800;

class Keyboard {
	char bkey[256];
	char  key[256];
public:
	Keyboard() {
		for (int i = 0; i < 256; i++) { key[i] = 0; }
	}
	void update() {
		memcpy(bkey, key, 256);
		GetHitKeyStateAll(key);
	}
	bool is_click(int key_input) {
		return !bkey[key_input] && key[key_input];
	}
};

class Mouse {
	int bstate, state;
	int mx, my;

public:
	Mouse() {
		state = 0;
		mx = 0, my = 0;
	}
	void update() {
		bstate = state;
		state = GetMouseInput();
		GetMousePoint(&mx, &my);
	}
	bool is_click(char dir) {
		if (dir == 'L') {
			return !(bstate & MOUSE_INPUT_LEFT) && (state & MOUSE_INPUT_LEFT);
		}
		if (dir == 'R') {
			return !(bstate & MOUSE_INPUT_RIGHT) && (state & MOUSE_INPUT_RIGHT);
		}
		return false;
	}
	bool is_release(char dir) {
		if (dir == 'L') {
			return (bstate & MOUSE_INPUT_LEFT) && !(state & MOUSE_INPUT_LEFT);
		}
		if (dir == 'R') {
			return (bstate & MOUSE_INPUT_RIGHT) && !(state & MOUSE_INPUT_RIGHT);
		}
		return false;
	}
	bool is_push(char dir) {
		if (dir == 'L') {
			return (state & MOUSE_INPUT_LEFT);
		}
		if (dir == 'R') {
			return (state & MOUSE_INPUT_RIGHT);
		}
		return false;
	}
	Point point() {
		return Point((double)mx, (double)my);
	}
};

class Rect {
public:
	double lx, ly, rx, ry;
};

class Line {
public:
	double sx, sy, gx, gy;
	Line(double sx, double sy, double gx, double gy) {
		this->sx = sx;
		this->sy = sy;
		this->gx = gx;
		this->gy = gy;
	}
	Line() {}
};

class Circle {
public:
	double x, y, r;
	Circle(double x, double y, double r) {
		this->x = x;
		this->y = y;
		this->r = r;
	}
	Circle() {}
};

class Goal {
public:
	double y;
};

class State {
	Rect board;
	vector<Line> lines;
	Circle ball;
	Goal goal;
	vector<Circle> items;

public:
	void update(double sx, double sy, double gx, double gy, int mode, double pixel_per_meter) {	//描画座標, 更新するもの, スケール
		sx /= pixel_per_meter;
		sy /= pixel_per_meter;
		gx /= pixel_per_meter;
		gy /= pixel_per_meter;
		
		if (mode == 0) {
			board.lx = min(sx, gx);
			board.ly = min(sy, gy);
			board.rx = max(sx, gx);
			board.ry = max(sy, gy);
		}
		else if (mode == 1) {
			lines.push_back(Line(sx, sy, gx, gy));
		}
		else if (mode == 2) {
			ball = Circle((sx + gx) / 2, (sy + gy) / 2, hypot(gx - sx, gy - sy) / 2);
		}
		else if (mode == 3) {
			goal.y = sy;
		}
		else {
			items.push_back(Circle((sx + gx) / 2, (sy + gy) / 2, hypot(gx - sx, gy - sy) / 2));
		}
	}

	void print(string filename, double pixel_per_meter, double item_move_speed) {
		ofstream ofs(filename);
		if (ofs.fail()) return;

		ofs << "PixelPerMeter" << " " << fixed << setprecision(6) << pixel_per_meter << endl;
		ofs << "Gravity" << " " << fixed << setprecision(6) << 9.8 << endl;
		ofs << "Ball" << " " << fixed << setprecision(6) << ball.x << " " << ball.y << " " << ball.r << endl;
		ofs << "Rect" << " " << fixed << setprecision(6) << board.lx << " " << board.ly << " " << board.rx << " " << board.ry << endl;
		ofs << "Line" << " " << lines.size() << endl;
		for (int i = 0; i < lines.size(); i++) {
			ofs << fixed << setprecision(6) << lines[i].sx << " " << lines[i].sy << " " << lines[i].gx << " " << lines[i].gy << endl;
		}
		ofs << "Goal" << " " << fixed << setprecision(6) << goal.y << endl;
		ofs << "GoalItem" << " " << items.size() << " " << item_move_speed << endl;
		for (int i = 0; i < items.size(); i++) {
			ofs << fixed << setprecision(6) << items[i].x << " " << items[i].y << " " << items[i].r << endl;
		}
	}

	void draw(double pixel_per_meter) {
		double scale = pixel_per_meter;

		DrawBox(board.lx * scale, board.ly * scale, board.rx * scale, board.ry * scale, GetColor(0, 0, 0), FALSE);
		for (int i = 0; i < lines.size(); i++) {
			DrawLine(lines[i].sx * scale, lines[i].sy * scale, lines[i].gx * scale, lines[i].gy * scale, GetColor(0, 0, 0), 2);
		}
		DrawCircle(ball.x * scale, ball.y * scale, ball.r * scale, GetColor(255, 0, 0), TRUE);

		DrawLine(0, goal.y * scale, WINDOW_SIZE_X, goal.y * scale, GetColor(0, 0, 0), 2);
		for (int i = 0; i < items.size(); i++) {
			DrawCircle(items[i].x * scale, items[i].y * scale, items[i].r * scale, GetColor(0, 0, 255), TRUE);
		}
	}
};

class Backup {
	stack<State> stk;

public:
	State pop() {
		if (stk.size() == 0) return State();
		if (stk.size() >= 2) stk.pop();
		return stk.top();
	}
	void push(State state) {
		stk.push(state);
	}
};

void draw_edit(double sx, double sy, double gx, double gy, int mode) {	//描画座標で与えられる
	if (mode == 0) {
		DrawBox(sx, sy, gx, gy, 0, FALSE);
	}
	if (mode == 1) {
		DrawLine(sx, sy, gx, gy, 0, 2);
	}
	if (mode == 2) {
		DrawCircle((sx + gx) / 2, (sy + gy) / 2, hypot(gx - sx, gy - sy) / 2, GetColor(255, 0, 0), TRUE);
	}
	if (mode == 3) {
		DrawLine(0, sy, WINDOW_SIZE_X, sy, 0, 2);
	}
	if (mode == 4) {
		DrawCircle((sx + gx) / 2, (sy + gy) / 2, hypot(gx - sx, gy - sy) / 2, GetColor(0, 0, 255), TRUE);
	}
}

Mouse mouse;
Keyboard keyboard;
double sx, sy;

State state;
Backup backup;

double pixel_per_meter = 50;
int mode = 0;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	ChangeWindowMode(TRUE);
	SetGraphMode(WINDOW_SIZE_X, WINDOW_SIZE_Y, 32);
	SetBackgroundColor(255, 255, 255);
	DxLib_Init();
	SetDrawScreen(DX_SCREEN_BACK);

	backup.push(state);

	while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0)
	{
		mouse.update();
		keyboard.update();
		
		if (mouse.is_click('L')) {
			sx = mouse.point().real();
			sy = mouse.point().imag();
		}
		if (mouse.is_release('L')) {
			state.update(sx, sy, mouse.point().real(), mouse.point().imag(), mode, pixel_per_meter);
			backup.push(state);
		}
		if (mouse.is_click('R') || keyboard.is_click(KEY_INPUT_W)) {
			state.print("map_data.txt", pixel_per_meter, 2);
		}
		if (keyboard.is_click(KEY_INPUT_SPACE)) {
			mode = (mode + 1) % 5;
		}
		if (keyboard.is_click(KEY_INPUT_Z) || keyboard.is_click(KEY_INPUT_BACK)) {
			state = backup.pop();
		}

		state.draw(pixel_per_meter);
		
		string s[5] = { "Rect", "Line", "Ball", "Goal", "Item" };
		DrawFormatString(600, 100, 0, "Edit : %s", s[mode].c_str());

		//編集中のやつを描く
		if (mouse.is_push('L')) {
			draw_edit(sx, sy, mouse.point().real(), mouse.point().imag(), mode);
		}
	}

	DxLib_End();
	return 0;
}