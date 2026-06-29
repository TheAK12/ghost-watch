#pragma once
#include <ftxui/screen/color.hpp>

using namespace ftxui;

struct Theme {
  Color bg = Color::RGB(0, 0, 0);
  Color fg = Color::RGB(255, 255, 255);
  Color fg_dim = Color::RGB(142, 142, 147);
  Color primary = Color::RGB(10, 132, 255);
  Color secondary = Color::RGB(94, 92, 230);
  Color success = Color::RGB(48, 209, 88);
  Color warning = Color::RGB(255, 159, 10);
  Color danger = Color::RGB(255, 69, 58);
  Color surface0 = Color::RGB(28, 28, 30);
  Color surface1 = Color::RGB(44, 44, 46);
  Color surface2 = Color::RGB(58, 58, 60);
  Color overlay = Color::RGB(142, 142, 147);
  Color teal = Color::RGB(100, 210, 255);
  Color peach = Color::RGB(255, 55, 95);
};

inline Theme &get_theme() {
  static Theme t;
  return t;
}
