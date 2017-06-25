//マップを読み込み, ゲームをする。バーにボールが当たったら終了。
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

namespace Complex {
	double dot(Point &a, Point &b) {
		return a.real() * b.real() + a.imag() * b.imag();
	}

	double cross(Point &a, Point &b) {
		return a.real() * b.imag() - a.imag() * b.real();
	}
}

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
	bool is_push(int key_input) {
		return key[key_input];
	}
};

class Time {
	clock_t bnow, now;
	int clocks;

public:
	void init() {
		now = clock();
		clocks = 0;
	}

	Time() {
		init();
	}

	void update() {
		bnow = now;
		now = clock();
		clocks += (now - bnow);
	}

	double byo() {
		return (double)clocks / CLOCKS_PER_SEC;
	}

	double delta_byo() {
		return (double)(now - bnow) / CLOCKS_PER_SEC;
	}
};

class Rect {
public:
	double lx, ly, rx, ry;
	double theta;			//ラジアン

	Rect() {
		theta = 0;
	}

	void update(double rad) {
		theta += rad;
	}

	Point point(int id) {
		Point ret;
		if (id == 0) {
			ret = Point(lx, ly);
		}
		if (id == 1) {
			ret = Point(lx, ry);
		}
		if (id == 2) {
			ret = Point(rx, ry);

		}
		if (id == 3) {
			ret = Point(rx, ly);
		}

		//長方形の中心を中心にして回転
		ret -= Point((lx + rx) / 2, (ly + ry) / 2);
		ret *= exp(theta * Point(0, 1));
		ret += Point((lx + rx) / 2, (ly + ry) / 2);
		return ret;
	}
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

	//線分の長さ
	double length()
	{
		return abs(Point(sx, sy) - Point(gx, gy));
	}

	//点と線分の距離
	double dist(Point & p)
	{
		Point s = Point(sx, sy);
		Point e = Point(gx, gy);

		if (Complex::dot(e - s, p - s) <= 0) return abs(p - s);
		if (Complex::dot(s - e, p - e) <= 0) return abs(p - e);
		return abs(Complex::cross(e - s, p - s)) / length();
	}

	//回転
	void rotate(double rad, Point center) {
		Point s = Point(sx, sy);
		Point e = Point(gx, gy);

		s -= center;
		s *= exp(Point(0, 1) * rad);
		s += center;

		e -= center;
		e *= exp(Point(0, 1) * rad);
		e += center;

		sx = s.real();
		sy = s.imag();
		gx = e.real();
		gy = e.imag();
	}

	Point s() { return Point(sx, sy); }
	Point e() { return Point(gx, gy); }
};

class Circle {
public:
	double x, y, r;
	Point speed;		//速度ベクトル

	//アイテムで使う
	int photo_handle;
	int score;

	Circle(double x, double y, double r) {
		this->x = x;
		this->y = y;
		this->r = r;
		this->speed = Point(0, 0);
	}
	Circle() {}

	void move(double byo) {
		x += speed.real() * byo;
		y += speed.imag() * byo;
	}
};

class Goal {
public:
	double y;
};

class State {
	double pixel_per_meter;
	double gravity;
	double item_move_speed;
	Rect board;
	vector<Line> lines;
	Circle ball;
	Goal goal;
	vector<Circle> items;

public:

	void input(string filename, vector<int> &photo_handles, vector<int> &scores) {
		ifstream ifs(filename);
		string trush;
		int num;

		if (ifs.fail()) return;

		ifs >> trush >> pixel_per_meter;
		ifs >> trush >> gravity;
		ifs >> trush >> ball.x >> ball.y >> ball.r;
		ifs >> trush >> board.lx >> board.ly >> board.rx >> board.ry;
		ifs >> trush >> num;

		lines.resize(num);
		for (int i = 0; i < num; i++) {
			ifs >> lines[i].sx >> lines[i].sy >> lines[i].gx >> lines[i].gy;
		}

		ifs >> trush >> goal.y;
		ifs >> trush >> num >> item_move_speed;

		items.resize(num);
		for (int i = 0; i < num; i++) {
			ifs >> items[i].x >> items[i].y >> items[i].r;
		}

		//画像と得点
		for (int i = 0; i < num; i++) {
			int id = rand() % scores.size();
			items[i].photo_handle = photo_handles[id];
			items[i].score = scores[id];
		}
	}

	void draw() {
		double scale = pixel_per_meter;

		Point p[4];
		for (int i = 0; i < 4; i++) { p[i] = board.point(i); p[i] *= scale; }
		DrawQuadrangle(p[0].real(), p[0].imag(), p[1].real(), p[1].imag(), p[2].real(), p[2].imag(), p[3].real(), p[3].imag(), GetColor(0, 0, 0), FALSE);

		for (int i = 0; i < lines.size(); i++) {
			DrawLine(lines[i].sx * scale, lines[i].sy * scale, lines[i].gx * scale, lines[i].gy * scale, GetColor(0, 0, 0), 2);
		}
		DrawCircle(ball.x * scale, ball.y * scale, ball.r * scale, GetColor(255, 0, 0), TRUE);
		
		for (int i = 0; i < items.size(); i++) {
			DrawExtendGraph((items[i].x - items[i].r) * scale, (items[i].y - items[i].r) * scale, (items[i].x + items[i].r) * scale, (items[i].y + items[i].r) * scale, items[i].photo_handle, FALSE);
			//DrawCircle(items[i].x * scale, items[i].y * scale, items[i].r * scale, GetColor(0, 0, 255), TRUE);
		}

		DrawLine(0, goal.y * scale, WINDOW_SIZE_X, goal.y * scale, GetColor(0, 0, 0), 2);
	}

	void move_map(double theta, double dt) {
		board.update(theta);

		Point center = Point((board.lx + board.rx) / 2, (board.ly + board.ry) / 2);

		for (int i = 0; i < lines.size(); i++) {
			lines[i].rotate(theta, center);
		}

		for (int i = 0; i < items.size(); i++) {
			items[i].x += item_move_speed * dt;
			if (items[i].x >= WINDOW_SIZE_X / pixel_per_meter + items[i].r) {
				items[i].x = -items[i].r;
			}
		}

		//ボールの位置も変える！！！（線分にめり込むと大変）
		Point p = Point(ball.x, ball.y);
		p -= center;
		p *= exp(Point(0, 1) * theta);
		p += center;
		ball.x = p.real();
		ball.y = p.imag();
	}

	void move_ball(double dt) {
		int i;

		double min_dist = 11451419;
		int id = -1;
		for (i = 0; i < lines.size(); i++) {
			double dist = lines[i].dist(Point(ball.x, ball.y));
			if (min_dist > dist) {
				min_dist = dist;
				id = i;
			}
		}
		if (min_dist > ball.r) {	//自由落下 (y軸が下を向いているので, y軸正方向に速度を上げる(重要！！)）
			ball.speed += Point(0, gravity * dt);
		}
		else {
			//線分id上をボールが転がる (速度の向きdir)
			Point dir;

			Point s = lines[id].s();
			Point e = lines[id].e();
			if (s.imag() > e.imag()) {	//sが（表示において）上, eが下になるようにする。
				swap(s, e);
			}

			dir = (e - s) / abs(e - s);	//s->eと転がる（上から下に転がる)


			if (Complex::dot(ball.speed, dir) > 0) {
				ball.speed = dir * abs(ball.speed);
			}
			else {
				ball.speed = -dir * abs(ball.speed);
			}
			ball.speed += dir * gravity * dt * dir.imag();
		}
		ball.move(dt);
	}

	bool hit_item() {
		for (int i = 0; i < items.size(); i++) {
			if (hypot(ball.x - items[i].x, ball.y - items[i].y) <= ball.r + items[i].r) {
				return true;
			}
		}
		return false;
	}
	int hit_item_score() {
		for (int i = 0; i < items.size(); i++) {
			if (hypot(ball.x - items[i].x, ball.y - items[i].y) <= ball.r + items[i].r) {
				return items[i].score;
			}
		}
		return 0;
	}

	bool hit_bar() {
		return ball.y + ball.r > goal.y;
	}
};

Time timer;
Keyboard keyboard;
double sx, sy;
State ini_state;
State state;
int retry_cnt;
int score;

const double wait_byo = 2;

vector<int> photo_handles;
vector<int> scores;

void move() {
	double dt = timer.delta_byo();
	double omega = 0.5;
	double theta;

	if (keyboard.is_push(KEY_INPUT_LEFT)) {
		theta = -omega * dt;
	}
	else if (keyboard.is_push(KEY_INPUT_RIGHT)) {
		theta = omega * dt;
	}
	else {
		theta = 0;
	}

	state.move_map(theta, dt);
	state.move_ball(dt);
}

void info_draw() {
	static int font200 = -1;
	if (font200 == -1) font200 = CreateFontToHandle(NULL, 200, 6);

	if (timer.byo() < wait_byo) {
		int t = (wait_byo - timer.byo()) + 0.9999;
		DrawFormatStringToHandle(WINDOW_SIZE_X / 2 - 50, WINDOW_SIZE_Y / 2 - 100, 0, font200, "%d", t);
	}

	DrawFormatString(200, 740, 0, "←→キーで傾ける.");
	DrawFormatString(500, 740, 0, "SPACEでリトライ");
	DrawFormatString(600, 10, 0, "ライフ = %d", retry_cnt);
	DrawFormatString(600, 35, 0, "進級まで%d単位", score);
}

void load_photo(vector<int> &photo_handles, vector<int> &scores) {
	photo_handles.push_back(LoadGraph("sotsuken.png"));  scores.push_back(2);
	photo_handles.push_back(LoadGraph("sannsuu.png"));    scores.push_back(2);
	photo_handles.push_back(LoadGraph("gunma.png"));     scores.push_back(0);
	photo_handles.push_back(LoadGraph("nihonnshi.png")); scores.push_back(1);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	ChangeWindowMode(TRUE);
	SetGraphMode(WINDOW_SIZE_X, WINDOW_SIZE_Y, 32);
	SetBackgroundColor(255, 255, 255);
	DxLib_Init();
	SetDrawScreen(DX_SCREEN_BACK);

	load_photo(photo_handles, scores);

	ini_state.input("map_data.txt", photo_handles, scores);
	state = ini_state;

	retry_cnt = 4;
	score = 3;

	bool is_catch_item = false;						//リトライ時に初期化
	int font80 = CreateFontToHandle(NULL, 80, 5);	//ゲームオーバー, ゲームクリアのフォント

	while (ScreenFlip() == 0 && ProcessMessage() == 0 && ClearDrawScreen() == 0)
	{
		keyboard.update();
		timer.update();

		//エスケープで終了
		if (keyboard.is_click(KEY_INPUT_ESCAPE)) {
			break;
		}

		//ボールがアイテムに触れたら単位を与える
		if (state.hit_item() && !is_catch_item) {
			score -= state.hit_item_score();
			is_catch_item = true;
		}

		//ゲームクリア
		if (score <= 0) {
			DrawFormatStringToHandle(WINDOW_SIZE_X / 2 - 40 * 2.5, WINDOW_SIZE_Y / 2 - 40, GetColor(255, 0, 0), font80, "進級!");
			DrawFormatString(300, 500, 0, "ライフ = %d\n", retry_cnt);
			DrawFormatString(300, 540, 0, "SPACEキーでリトライ");
			if (keyboard.is_click(KEY_INPUT_SPACE)) {
				state = ini_state;
				timer.init();
				retry_cnt = 4;
				score = 3;
				is_catch_item = false;
			}
		}

		//ボールがバーを超えたらリトライ (リトライ回数0でゲームオーバー）
		else if (state.hit_bar()) {
			if (retry_cnt > 1) {
				state = ini_state;
				timer.init();

				//アイテム（単位）を入手していないときだけ, ライフが減るようにする。
				if (!is_catch_item) {
					retry_cnt--;
				}
				is_catch_item = false;
			}
			else {
				DrawFormatStringToHandle(WINDOW_SIZE_X / 2 - 40 * 2.5, WINDOW_SIZE_Y / 2 - 40, GetColor(255, 0, 0), font80, "留年!");
				DrawFormatString(300, 500, 0, "SPACEキーでリトライ");
				if (keyboard.is_click(KEY_INPUT_SPACE)) {
					state = ini_state;
					timer.init();
					retry_cnt = 4;
					score = 3;
					is_catch_item = false;
				}
			}
		}

		if (timer.byo() > wait_byo) {
			move();
		}

		state.draw();
		info_draw();
	}

	DxLib_End();
	return 0;
}