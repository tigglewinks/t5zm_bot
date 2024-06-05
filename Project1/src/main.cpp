#define NOMINMAX

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#include <Windows.h>
#include <dwmapi.h>
#include <d3d11.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_impl_win32.h>

#include "offsets.h"
#include <MemMan.h>
#include "helpers.h"

constexpr int selection_change_delay_ms = 150;

const auto aim_height = 55; // units up from origin, 60 is eyelevel
const auto aim_fov = 100;
const auto aim_smooth = 20;
const auto esp_width = 50; // units in each direction (unused)


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
	if (ImGui_ImplWin32_WndProcHandler(window, message, w_param, l_param)) {
		return 0L;
	}

	if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0L;
	}

	return DefWindowProc(window, message, w_param, l_param);
}

void draw_outlined_text(const ImVec2 &pos, ImU32 col, const char *text) {
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x - 1, pos.y), ImColor(0.f, 0.f, 0.f, 255.f), text);
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x + 1, pos.y), ImColor(0.f, 0.f, 0.f, 255.f), text);
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x, pos.y - 1), ImColor(0.f, 0.f, 0.f, 255.f), text);
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x, pos.y + 1), ImColor(0.f, 0.f, 0.f, 255.f), text);

	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x - 1, pos.y - 1), ImColor(0.f, 0.f, 0.f, 255.f), text);
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x + 1, pos.y + 1), ImColor(0.f, 0.f, 0.f, 255.f), text);
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x - 1 , pos.y + 1), ImColor(0.f, 0.f, 0.f, 255.f), text);
	ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos.x + 1, pos.y - 1), ImColor(0.f, 0.f, 0.f, 255.f), text);

	ImGui::GetBackgroundDrawList()->AddText(pos, col, text);
}

void draw_centered_text(const char* text, ImVec2 position, ImU32 color) {
	// Get the current font
	ImFont* font = ImGui::GetFont();

	// Calculate the width of the text
	ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, text);

	// Calculate the position for center alignment
	ImVec2 centeredPos = ImVec2(position.x - textSize.x * 0.5f, position.y);

	draw_outlined_text(centeredPos, color, text);
}

void draw_outlined_box(const ImVec2& pos1, const ImVec2& pos2, ImU32 col) {
	// ImVec2(coords), ImVec2(coords2), IM_COL32(color)
	ImVec2 p1 = ImVec2(floor(pos1.x), floor(pos1.y));
	ImVec2 p2 = ImVec2(floor(pos2.x), floor(pos2.y));

	// Draw outlines
	ImGui::GetBackgroundDrawList()->AddRect(ImVec2(p1.x - 1, p1.y), ImVec2(p2.x - 1, p2.y), IM_COL32(0, 0, 0, 128));
	ImGui::GetBackgroundDrawList()->AddRect(ImVec2(p1.x + 1, p1.y), ImVec2(p2.x + 1, p2.y), IM_COL32(0, 0, 0, 128));
	ImGui::GetBackgroundDrawList()->AddRect(ImVec2(p1.x, p1.y - 1), ImVec2(p2.x, p2.y - 1), IM_COL32(0, 0, 0, 128));
	ImGui::GetBackgroundDrawList()->AddRect(ImVec2(p1.x, p1.y + 1), ImVec2(p2.x, p2.y + 1), IM_COL32(0, 0, 0, 128));

	// Draw main box
	ImGui::GetBackgroundDrawList()->AddRect(p1, p2, col);


}

void draw_filled_box(const ImVec2& pos1, const ImVec2& pos2, ImU32 col) {
	// ImVec2(coords), ImVec2(coords2), IM_COL32(color)
	ImVec2 p1 = ImVec2(floor(pos1.x), floor(pos1.y));
	ImVec2 p2 = ImVec2(floor(pos2.x), floor(pos2.y));

	// Draw outlines
	ImGui::GetBackgroundDrawList()->AddRectFilled(p1, p2, col);


}

auto get_feature_color(bool feature) {
	if (feature)
		return IM_COL32(111, 208, 0, 255);
	else
		return IM_COL32(160, 161, 164, 255);
}

void calculateBounds(const Vec2& pos2, const Vec2& pos3, ImVec2& topLeft, ImVec2& bottomRight) {
	topLeft.x = std::min(pos2.X, pos3.X);
	topLeft.y = std::min(pos2.Y, pos3.Y);
	bottomRight.x = std::max(pos2.X, pos3.X);
	bottomRight.y = std::max(pos2.Y, pos3.Y);
}

float distance(int x1, int y1, int x2, int y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

float vec_distance(Vec3 p1, Vec3 p2) {
	return sqrt(pow(p2.X - p1.X, 2) + pow(p2.Y - p1.Y, 2));
}

// Function to get current mouse position and calculate distance to a specified point
float distance_from_mouse(int x, int y) {
	POINT cursor_pos;
	GetCursorPos(&cursor_pos);
	return distance(cursor_pos.x, cursor_pos.y, x, y);
}

Vec3 offsetPosition(const Vec3& pos1, const Vec3& pos2, int distance) {
	// Calculate the direction vector from pos1 to pos2
	float dx = pos2.X - pos1.X;
	float dy = pos2.Y - pos1.Y;

	float perpX = -dy;
	float perpY = dx;

	// Calculate the magnitude of the direction vector
	float magnitude = sqrt(dx * dx + dy * dy);

	// Normalize the direction vector
	perpX /= magnitude;
	perpY /= magnitude;

	// Offset the position by the given distance along the direction vector
	Vec3 offsetPos;
	offsetPos.X = pos2.X + dx * distance;
	offsetPos.Y = pos2.Y + dy * distance;
	offsetPos.Z = pos2.Z; // Keep the same z coordinate

	return offsetPos;
}

std::string round_float_str(float value, int precision) {
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(precision) << value;
	return oss.str();
}

float units_to_meters(float value) {
	return value / 30.62117333f;
}


INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show) {
	WNDCLASSEXW wc{};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = L"eoc";

	RegisterClassExW(&wc);

	const HWND window = CreateWindowExW(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
		wc.lpszClassName,
		L"",
		WS_POPUP,
		0,
		0,
		2560,
		1080,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	SetLayeredWindowAttributes(window, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	

	RECT client_area{};
	GetClientRect(window, &client_area);

	RECT window_area{};
	GetWindowRect(window, &window_area);

	POINT diff{};
	ClientToScreen(window, &diff);

	const MARGINS margins{
		window_area.left + (diff.x - window_area.left),
		window_area.top + (diff.y - window_area.top),
		client_area.right,
		client_area.bottom
	};

	DwmExtendFrameIntoClientArea(window, &margins);
	

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};

	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer) {
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	} else {
		return 1;
	}

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX11_Init(device, device_context);

	// Get ready for menu

	ImU32 active_color = IM_COL32(111, 208, 0, 255);
	ImU32 inactive_color = IM_COL32(160, 161, 164, 255);
	ImU32 xhair_color = IM_COL32(207, 251, 0, 255);
	ImU32 esp_color = IM_COL32(126, 211, 2, 255);
	ImU32 hp_high_color = IM_COL32(32, 222, 32, 200);
	ImU32 hp_mid_color = IM_COL32(190, 174, 8, 200);
	ImU32 hp_low_color = IM_COL32(223, 1, 29, 200);

	int selected_feature = 0;
	int num_features = 5;

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = NULL;
	
	ImFont* skeetfont = io.Fonts->AddFontFromFileTTF("font.ttf", 8.f);

	bool running = true;

	auto last_selection_change_time = std::chrono::steady_clock::now();
	auto last_toggle_change_time = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();

	bool crosshair_enabled = true;
	bool zombie_esp_enabled = false;
	bool aimbot_enabled = false;
	bool debug_enabled = true;

	MemMan memory_manager;
	uintptr_t game_process = memory_manager.getProcess(L"BlackOps.exe");
	std::cout << "Game process: " << game_process << '\n';
	if (game_process == 0) {
		std::cout << "game not found! open game first\n";
		return -1;
	}

	while (running) {
		now = std::chrono::steady_clock::now();

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) {
				running = false;
			}
		}

		if (!running) {
			break;
		}

		static bool menu_visible = true;
		if (GetAsyncKeyState(VK_INSERT) & 0x8000) {
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_selection_change_time).count() >= selection_change_delay_ms) {
				menu_visible = !menu_visible;
				last_selection_change_time = now;
			}
		}

		//  loop
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();
		

		// Start  logic after this line
		if (menu_visible) {

			// Keyboard input handling
			if (GetAsyncKeyState(VK_UP) & 0x8000) {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_selection_change_time).count() >= selection_change_delay_ms) {
					selected_feature = (selected_feature - 1 + num_features) % num_features; // Decrement selected_feature
					last_selection_change_time = now;
				}
			} else if (GetAsyncKeyState(VK_DOWN) & 0x8000) {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_selection_change_time).count() >= selection_change_delay_ms) {
					selected_feature = (selected_feature + 1) % num_features; // Increment selected_feature
					last_selection_change_time = now;
				}
			} else if (GetAsyncKeyState(VK_RIGHT) & 0x8000) {
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_toggle_change_time).count() >= selection_change_delay_ms) {
					switch (selected_feature) {
					case 0: // CROSSHAIR
						crosshair_enabled = !crosshair_enabled;
						break;
					case 1: // ZOMBIE
						zombie_esp_enabled = !zombie_esp_enabled;
						break;
					case 2: // AIMBOT
						aimbot_enabled = !aimbot_enabled;
						break;
					case 3: // DEBUG
						debug_enabled = !debug_enabled;
						break;
					default:
						break; 
					}
					last_toggle_change_time = now;
				}
			}

			ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(21, 21), ImVec2(129, 348), IM_COL32(0, 0, 0, 64)); // Background
			ImGui::GetBackgroundDrawList()->AddRect(ImVec2(20, 20), ImVec2(130, 348), IM_COL32(0, 0, 0, 255)); // Outline

			draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 1.f), get_feature_color(crosshair_enabled), "CROSSHAIR");
			draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 2.f), get_feature_color(zombie_esp_enabled), "ZOMBIE");
			draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 3.f), get_feature_color(aimbot_enabled), "AIMBOT F");
			draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 4.f), get_feature_color(debug_enabled), "DEBUG");
			draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 5.f), inactive_color, "NO SPREAD");

			

			// Feature selector

			ImGui::GetBackgroundDrawList()->AddRect(ImVec2(30.f, 41.f + 12.f * static_cast<float>(selected_feature)), ImVec2(36.f, 41.f + 12.f * static_cast<float>(selected_feature) + 6.f), IM_COL32(0, 0, 0, 255)); // Outline
			ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(31.f, 42.f + 12.f * static_cast<float>(selected_feature)), ImVec2(35.f, 42.f + 12.f * static_cast<float>(selected_feature) + 4.f), active_color); // Square
		}

		

		

		// Rendering
		
		if (zombie_esp_enabled || aimbot_enabled) {
			// Get view matrix
			ViewMatrix view_matrix = memory_manager.readMem<ViewMatrix>(Zombies::A_ViewMatrix);
			uintptr_t local_pointer = memory_manager.readMem<uintptr_t>(Zombies::A_EntityList);
			Vec3 local_position = memory_manager.readMem<Vec3>(local_pointer + Zombies::Offsets::o_location);
			POINT cursor_pos;
			GetCursorPos(&cursor_pos);
			int screen_center_x = cursor_pos.x;
			int screen_center_y = cursor_pos.y;
			float radius_x = aim_fov * 2;
			float radius_y = aim_fov * 2;

			// Loop through EntityList
			int num_zombies = 0;
			ImVec2 best_target { 9999, 9999 };
			Vec3 best_target_3d{ 9999, 9999, 9999 };
			int best_target_ent = -1; 
			for (short int i = 1; i < 30; ++i) { // i set to 1 to skip over first entity in the list (us)
				uintptr_t entity = memory_manager.readMem<uintptr_t>(Zombies::A_EntityList + i * Zombies::Offsets::o_distance_between);
				if (entity == NULL) continue;

				// Check if valid entity
				int health = memory_manager.readMem<int>(entity + Zombies::Offsets::o_health);
				int max_health = memory_manager.readMem<int>(entity + Zombies::Offsets::o_max_health);
				int type_test = memory_manager.readMem<int>(entity + Zombies::Offsets::o_type_test);
				int type_test2 = memory_manager.readMem<int>(entity + Zombies::Offsets::o_type_test2);
				int type_test3 = memory_manager.readMem<int>(entity + Zombies::Offsets::o_type_test3);
				Vec3 position = memory_manager.readMem<Vec3>(entity + Zombies::Offsets::o_location);
				float ent_distance = vec_distance(local_position, position);
				if (health <= 0) continue;
				if (max_health <= 100) continue; // Zombies have a default health higher than 100
				if (ent_distance >= 10000) continue;


				num_zombies += 1;

				// Get Other Entity Locations

				Vec3 position2 = memory_manager.readMem<Vec3>(entity + Zombies::Offsets::o_location2);
				Vec3 position3 = memory_manager.readMem<Vec3>(entity + Zombies::Offsets::o_location3);
				Vec3 position4 = memory_manager.readMem<Vec3>(entity + Zombies::Offsets::o_location4);

				float height = position3.Y - position2.Y;

				if (aimbot_enabled && GetAsyncKeyState(0x05) & 0x8000) { // 0x46 = F
					Vec3 target_position = position;

					if (type_test2 == 1103101952) { // SPECIAL
						target_position.Z += 25.f;
					}
					else if (type_test == 2048 || type_test == 264192) { // SPECIAL
						target_position.Z += 25.f;
					}
					else if ((int)round(height) == 50) { // MONKEY
						target_position.Z += 10.f;
					} else {
						target_position.Z += aim_height;
					}

					Vec2 screen_origin;
					if (WorldToScreen(target_position, screen_origin, view_matrix.Matrix)) {
						ImVec2 pos = { screen_origin.X, screen_origin.Y };

						float entity_distance_from_xhair = distance_from_mouse(pos.x, pos.y);;

						if (pos.x >= screen_center_x - radius_x && pos.x <= screen_center_x + radius_x &&
							pos.y >= screen_center_y - radius_y && pos.y <= screen_center_y + radius_y) {
							float entity_distance_from_xhair = distance_from_mouse(pos.x, pos.y);

							// Check if this target is closer than the current best target
							if (ent_distance < vec_distance(local_position, best_target_3d)) {
								best_target = pos;
								best_target_3d = target_position;
								best_target_ent = entity;
							}
						}

						/* closest to me
						if (pos.x >= screen_center_x - radius_x && pos.x <= screen_center_x + radius_x && pos.y >= screen_center_y - radius_y && pos.y <= screen_center_y + radius_y  && ent_distance < vec_distance(local_position, best_target_3d)) {
							best_target = pos;
							best_target_3d = target_position;
							best_target_ent = entity;
						}
						else if (pos.x >= screen_center_x - radius_x && pos.x <= screen_center_x + radius_x && pos.y >= screen_center_y - radius_y && pos.y <= screen_center_y + radius_y && best_target_ent == entity) {
							best_target = pos;
							best_target_3d = target_position;
							best_target_ent = entity;
						} */

						/* Closest to crosshair target selection
						if (entity_distance_from_xhair < distance_from_mouse(best_target.x, best_target.y)) {
							best_target = pos;
						}
						*/
						
					}
				}
				
				// regular esp using pos2 and pos3
				if (zombie_esp_enabled) {
					int entity_type = memory_manager.readMem<uint16_t>(entity + Zombies::Offsets::o_entity_type);
					float height = position3.Y - position2.Y;
					Vec2 screen_coords2, screen_coords3;
					if (WorldToScreen(position2, screen_coords2, view_matrix.Matrix) && WorldToScreen(position3, screen_coords3, view_matrix.Matrix)) {
						ImVec2 top_left_bounds;
						ImVec2 bottom_right_bounds;
						calculateBounds(screen_coords2, screen_coords3, top_left_bounds, bottom_right_bounds);
						//draw_outlined_text(coords, active_color, "2");
						std::string height_flag = "H: " + std::to_string((int)round(height));
						std::string type_flag = "T2: " + std::to_string(type_test2);
						std::string type3_flag = "T3: " + std::to_string(type_test3);
						std::string distance_text = round_float_str(units_to_meters(ent_distance), 0) + " M";
						std::string zombie_type = "ZOMBIE";
						ImU32 esp_col = esp_color;

						if (type_test2 == 1103101952) {
							zombie_type = "SPECIAL";
							esp_col = IM_COL32(230, 204, 60, 255);
						}
						else if (type_test == 2048 || type_test == 264192) {
							zombie_type = "SPECIAL";
							esp_col = IM_COL32(230, 204, 60, 255);
						}
						else if ((int)round(height) == 50) {
							zombie_type = "MONKEY";
							esp_col = IM_COL32(230, 204, 60, 255);
						}
						else {
							zombie_type = "ZOMBIE"; // 133120, 
							esp_col = esp_color;
						}

						// Draw regular box
						draw_outlined_box(top_left_bounds, bottom_right_bounds, esp_col);

						draw_centered_text(zombie_type.c_str(), ImVec2(((top_left_bounds.x + bottom_right_bounds.x) / 2), top_left_bounds.y - 12), IM_COL32(255, 255, 255, 255));

						if (debug_enabled) {
							draw_outlined_text(ImVec2(bottom_right_bounds.x + 3, top_left_bounds.y), IM_COL32(255, 255, 255, 255), std::to_string(type_test).c_str());
							draw_outlined_text(ImVec2(bottom_right_bounds.x + 3, top_left_bounds.y + 6 * 1), IM_COL32(255, 255, 255, 255), height_flag.c_str());
							draw_outlined_text(ImVec2(bottom_right_bounds.x + 3, top_left_bounds.y + 6 * 2), IM_COL32(255, 255, 255, 255), type_flag.c_str());
							draw_outlined_text(ImVec2(bottom_right_bounds.x + 3, top_left_bounds.y + 6 * 3), IM_COL32(255, 255, 255, 255), type3_flag.c_str());
						}
						draw_centered_text(distance_text.c_str(), ImVec2(((top_left_bounds.x + bottom_right_bounds.x) / 2) + 1, bottom_right_bounds.y - 1), IM_COL32(255, 255, 255, 255));

						// Health bar logic
						float hp_percent = static_cast<float>(health) / static_cast<float>(max_health);

						// Draw hp bar background
						draw_filled_box(ImVec2(top_left_bounds.x - 7, top_left_bounds.y - 1), ImVec2(top_left_bounds.x - 3, bottom_right_bounds.y + 1), IM_COL32(0, 0, 0, 128));

						float bar_height = (bottom_right_bounds.y - top_left_bounds.y) * hp_percent;

						if (hp_percent < 0.333f) {
							// 33% HP bar
							ImVec2 bar_top_left = ImVec2(top_left_bounds.x - 6, bottom_right_bounds.y - bar_height);
							draw_filled_box(bar_top_left, ImVec2(top_left_bounds.x - 4, bottom_right_bounds.y), hp_low_color);
						}
						else if (hp_percent < 0.666f) {
							// 66% HP bar
							ImVec2 bar_top_left = ImVec2(top_left_bounds.x - 6, bottom_right_bounds.y - bar_height);
							draw_filled_box(bar_top_left, ImVec2(top_left_bounds.x - 4, bottom_right_bounds.y), hp_mid_color);
						}
						else {
							// 100% HP bar
							ImVec2 bar_top_left = ImVec2(top_left_bounds.x - 6, bottom_right_bounds.y - bar_height);
							draw_filled_box(bar_top_left, ImVec2(top_left_bounds.x - 4, bottom_right_bounds.y), hp_high_color);
						}
						
					}
				}
			}

			if (aimbot_enabled) {
				if (best_target_ent != -1) {
				//if (best_target.x >= screen_center_x - radius_x && best_target.x <= screen_center_x + radius_x && best_target.y >= screen_center_y - radius_y && best_target.y <= screen_center_y + radius_y && units_to_meters(vec_distance(local_position, best_target_3d)) <= 300.f) {
					double distance_x = static_cast<double>(best_target.x) - screen_center_x;
					double distance_y = static_cast<double>(best_target.y) - screen_center_y;

					distance_x /= aim_smooth;
					distance_y /= aim_smooth;

					draw_outlined_box(ImVec2(best_target.x - 2, best_target.y - 2), ImVec2(best_target.x + 2, best_target.y + 2), esp_color);
					mouse_event(MOUSEEVENTF_MOVE, (DWORD)distance_x, (DWORD)distance_y, NULL, NULL);
				}
			}
			
			if (debug_enabled) {
				std::string display_text = "ZOMBIES: " + std::to_string(num_zombies);
				std::string display_text_x = "X: " + std::to_string((int)local_position.X);
				std::string display_text_y = "Y: " + std::to_string((int)local_position.Y);
				std::string display_text_z = "Z: " + std::to_string((int)local_position.Z);
				draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 7), active_color, display_text.c_str());
				draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 8), active_color, display_text_x.c_str());
				draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 9), active_color, display_text_y.c_str());
				draw_outlined_text(ImVec2(39.f, 27.f + 12.f * 10), active_color, display_text_z.c_str());
			}
			
		}

		// Crosshair
		if (crosshair_enabled) {
			int screenWidth = GetSystemMetrics(SM_CXSCREEN);
			int screenHeight = GetSystemMetrics(SM_CYSCREEN);
			
			int center_width = screenWidth / 2;
			int center_height = screenHeight / 2;

			float start_x = static_cast<float>(center_width - 3);
			float start_y = static_cast<float>(center_height - 3);

			ImGui::GetBackgroundDrawList()->AddRect(ImVec2(start_x, start_y), ImVec2(start_x + 5.f, start_y + 5.f), IM_COL32(0, 0, 0, 255)); // Outline
			ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(start_x + 1.f, start_y + 1.f), ImVec2(start_x + 4.f, start_y + 4.f), xhair_color); // Inside


		}
		

		ImGui::Render();

		constexpr float color[4]{ 0.f, 0.f, 0.f, 0.f };
		device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
		device_context->ClearRenderTargetView(render_target_view, color);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		swap_chain->Present(1U, 0U);
	}

	// Cleanup

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	if (swap_chain) {
		swap_chain->Release();
	}

	if (device_context) {
		device_context->Release();
	}

	if (device) {
		device->Release();
	}

	if (render_target_view) {
		render_target_view->Release();
	}

	DestroyWindow(window);
	UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}