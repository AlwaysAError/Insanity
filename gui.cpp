#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT });
	ImGui::Begin(
		"Insanity Cheat https://discord.gg/projectunban",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);

	if (ImGui::BeginTabBar("MainTabBar")) { // Start a tab bar
		// Tab Normals Tab
		if (ImGui::BeginTabItem("Normals Tab")) {
			ImGui::Text("This tab offers simple cheats.");
			ImGui::Text("   ");
			static bool godmode = false; // Persists across frames
			ImGui::Checkbox("GodMode", &godmode);
			static bool agodmode = false; // Persists across frames
			ImGui::Checkbox("Advanced GodMode", &agodmode);
			ImGui::Text("   ");
			static bool aimbot = false; // Persists across frames
			ImGui::Checkbox("AimBot", &aimbot);
			static bool psilent = false; // Persists across frames
			ImGui::Checkbox("Make AimBot P Silent", &psilent);
			static bool triggerbot = false; // Persists across frames
			ImGui::Checkbox("TriggerBot", &triggerbot);
			static float value1 = 0.0f; // The variable to modify with the slider
			ImGui::SliderFloat("TriggerBot Delay", &value1, 0.0f, 1000.0f, "%.1f");
			static bool nospread = false; // Persists across frames
			ImGui::Checkbox("NoSpread", &nospread);
			static bool NoRecoil = false; // Persists across frames
			ImGui::Checkbox("NoRecoil", &NoRecoil);
			ImGui::Text("   ");
			static bool InfiniteAmmo = false; // Persists across frames
			ImGui::Checkbox("Infinite Ammo", &InfiniteAmmo);
			static bool rapidfire = false; // Persists across frames
			ImGui::Checkbox("Rapid Fire", &rapidfire);
			ImGui::Text("   ");
			ImGui::Button("Teleport To Crosshair");
			ImGui::Text("   ");
			static bool massivemic = false; // Persists across frames
			ImGui::Checkbox("Massive Mic", &massivemic);
			ImGui::Text("Makes every player hear what you are saying no matter were you are");
			ImGui::EndTabItem();
		}
		// Tab Exploits Tab
		if (ImGui::BeginTabItem("Exploits Tab")) {
			ImGui::Text("This tab offers game exploit. Exploits may cause your account to be banned!");
			ImGui::Text("   ");
			static bool AntiAim = false; // Persists across frames
			ImGui::Checkbox("AntiAim", &AntiAim);
			ImGui::Text("Spin Speed");
			static float value2 = 0.0f; // The variable to modify with the slider
			ImGui::SliderFloat("Spin Speed", &value2, 0.0f, 100.0f, "%.1f");
			ImGui::Text("Pitch");
			static float value3 = 0.0f; // The variable to modify with the slider
			ImGui::SliderFloat("Pitch", &value3, 0.0f, 180.0f, "%.1f");
			ImGui::Text("   ");
			static bool Doubletap = false; // Persists across frames
			ImGui::Checkbox("Double Tap", &Doubletap);
			ImGui::Text("   ");
			ImGui::Text("Select A Bind For TimeStop");
			// List of items for the dropdown
			static const char* items[] = { "M1+M2", "P", "CONTROL", "Side Button" };
			static int current_item1 = 0; // Index of the currently selected item

			// Combo box
			ImGui::Combo("TimeStop", &current_item1, items, IM_ARRAYSIZE(items));
			// - "Select Option": Label for the combo box
			// - &current_item: Pointer to the index of the selected item
			// - items: Array of strings for the options
			// - IM_ARRAYSIZE(items): Number of items in the array

			ImGui::Text("   ");
			ImGui::Text("Perm Kicker");
			ImGui::Button("Kick All Hosts");
			ImGui::Button("Kick All Mods");
			ImGui::Button("Kick All Co Onwers");
			ImGui::Button("Kick Room Owner");
			ImGui::Button("Kick All With Perms");
			ImGui::Button("Kick All With No Perms");
			ImGui::Button("Kick Everyone But Room Owner");
			ImGui::Text("   ");
			ImGui::Text("Flying");
			static bool NoClip = false; // Persists across frames
			ImGui::Checkbox("NoClip", &NoClip);
			static float value4 = 0.0f; // The variable to modify with the slider
			ImGui::SliderFloat("NoClip Speed", &value4, 0.0f, 10000.0f, "%.1f");
			static bool CreatorFly = false; // Persists across frames
			ImGui::Checkbox("CreatorFly", &CreatorFly);
			static float value5 = 0.0f; // The variable to modify with the slider
			ImGui::SliderFloat("CreatorFly Speed", &value5, 0.0f, 10000.0f, "%.1f");
			ImGui::Text("   ");
			ImGui::Text("Message To Server.");
			ImGui::Text("Sends the input message to the hole server");
			static char buffer[256] = ""; // Buffer to store the input text (256 chars max)
			ImGui::InputText(" ", buffer, sizeof(buffer));
			// - "Name": Label for the input field
			// - buffer: Character array to store the input
			// - sizeof(buffer): Maximum buffer size

			ImGui::Text("You typed: %s", buffer); // Display the input
			ImGui::Button("Send Message");
			ImGui::EndTabItem();
		}
		// Tab Exetreme Exploits
		if (ImGui::BeginTabItem("Exetreme Exploits")) {
			ImGui::Text("WARNING THESE EXPLOITS MAY CAUSE A INSTANT BAN!");
			ImGui::Text("   ");
			ImGui::Text("Lobby Stuff");
			ImGui::Button("Soft Lock Lobby");
			ImGui::Button("Crash Lobby");
			ImGui::Button("DDoS Lobby . MAY TAKE YOUR INTERNET OFFLINE FOR 1 MINUTE");
			ImGui::Text("   ");
			ImGui::Text("Head Display");
			// List of items for the dropdown
			static const char* items[] = { "Vmod Text", "Mod Text", "Dev Text", "BroadCasting Text" };
			static int current_item = 0; // Index of the currently selected item

			// Combo box
			ImGui::Combo("Select Option", &current_item, items, IM_ARRAYSIZE(items));
			// - "Select Option": Label for the combo box
			// - &current_item: Pointer to the index of the selected item
			// - items: Array of strings for the options
			// - IM_ARRAYSIZE(items): Number of items in the array

			// Display the selected item
			ImGui::Text("Selected: %s", items[current_item]);
			ImGui::Text("   ");
			ImGui::Text("Lobby Host Exploits");
			ImGui::Button("Become lobby host");
			ImGui::Text("Forces RecRoom to make you the lobby host as the game is P2P");
			ImGui::Button("Become room owner");
			ImGui::Text("Gives you ownership of room for the lobby you are in. THE GREAT RETURN OF 2022 CHEATS");
			ImGui::Button("Makerpen all");
			ImGui::Text("Gives everyone full makerpen permissions");
			ImGui::Button("Swastika-fi lobby");
			ImGui::Text("Displays a swastika on everyone's screen");
			ImGui::EndTabItem();
		}
		// Tab Visuals
		if (ImGui::BeginTabItem("Visuals")) {
			ImGui::Text("This tab is used for visual elements.");
			ImGui::Text("   ");
			static bool pickupesp = false; // Persists across frames
			ImGui::Checkbox("Pick Up ESP", &pickupesp);
			static bool Playeresp = false; // Persists across frames
			ImGui::Checkbox("Player ESP", &Playeresp);
			ImGui::EndTabItem();
		}
		// Tab Players
		if (ImGui::BeginTabItem("Players")) {
			ImGui::Text("This tab is for targetting select players.");
			ImGui::Text("   ");
			ImGui::Text("We are not detecting any players in your lobby besides you!");
			ImGui::EndTabItem();
		}
		// Tab Lua Executor
		if (ImGui::BeginTabItem("Lua Executor")) {
			ImGui::Text("Our cheat not good enough or just want more feature? This tab will show all your");
			ImGui::Text("Lua scripts you have added to the configs folder. Use the configs folder button");
			ImGui::Text("below to access the configs folder were you can add your Lua scripts.");
			ImGui::Text("   ");
			ImGui::Button("Open Lua Scripts Folder");
			ImGui::Text("   ");
			ImGui::Text("You currently have no Lua scripts.");
			ImGui::EndTabItem();
		}
		// Tab Prefab Swapper
		if (ImGui::BeginTabItem("Prefab Swapper")) {
			ImGui::Text("Use this tab to swap game prefabs around.");
			ImGui::Text("A example would be swapping the camera around for a Laser Shotgun");
			ImGui::EndTabItem();
		}
		// Tab Misc Options
		if (ImGui::BeginTabItem("Misc Options")) {
			ImGui::Text("This tab offers options which did not fit into other tabs.");
			ImGui::Text("   ");
			ImGui::Text("Protections");
			static bool antikick = false; // Persists across frames
			ImGui::Checkbox("Anti Kick", &antikick);
			static bool antiban = false; // Persists across frames
			ImGui::Checkbox("Anti Ban", &antiban);
			static bool antivoiceban = false; // Persists across frames
			ImGui::Checkbox("Anti Voice Ban", &antivoiceban);
			static bool antisoftlock = false; // Persists across frames
			ImGui::Checkbox("Anti Soft Lock", &antisoftlock);
			static bool anticrash = false; // Persists across frames
			ImGui::Checkbox("Anti Crash", &anticrash);
			static bool antilag = false; // Persists across frames
			ImGui::Checkbox("Anti Lag", &antilag);
			static bool hideip = false; // Persists across frames
			ImGui::Checkbox("Hide IP", &hideip);
			ImGui::Text("   ");
			ImGui::Text("   ");
			ImGui::Text("   ");
			ImGui::Text("Test Boxes");

				// State to track toggle
				static bool isToggleOn = false;

				// Toggle button (using Checkbox)
				if (ImGui::Checkbox("Show Options", &isToggleOn)) {
					// Optional: Handle toggle change if needed
				}

				// Show selection menu if toggle is on
				if (isToggleOn) {
					// Combo box for selection
					static int currentItembox = 0;
					const char* itemsbox[] = { "Option 1", "Option 2", "Option 3", "Option 4" };
					ImGui::Combo("Select Option", &currentItembox, itemsbox, IM_ARRAYSIZE(itemsbox));
				}

				ImGui::EndTabItem();
			}
		// Tab Info/Credits
		if (ImGui::BeginTabItem("Info And Credits")) {
			ImGui::Text("A tab for the credits and any information you may need to know.");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Credits");
			ImGui::Text("   ");
			ImGui::Text("extremeblitz_ on Discord. Main coder");
			ImGui::Text("Cazz on YouTube. Helped with some ImGui elements");
			ImGui::Text("FatVirginCheater on Youtube. Helped with some features");
			ImGui::Text("spinmaster on Discord. Helped with some features");
			ImGui::Text("cheatzking on Discord. Helped EAC and Referee bypass");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Cheat Information");
			ImGui::Text("   ");
			ImGui::Text("Version 1.0");
			ImGui::Text("Build Type NORMAL");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Update Logs");
			ImGui::Text("   ");
			ImGui::Text("Updates? There are no updates your using V1.0 lol");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Help/Support");
			ImGui::Text("   ");
			ImGui::Text("How can I know if the bypass is up to date?");
			ImGui::Text("YOU DON'T!!! HA HA HA no but you can just ask in the Discord server or check messages sent");
			ImGui::Text("by other users.");
			ImGui::Text("   ");
			ImGui::Text("If the bypass is out of date will I get banned?");
			ImGui::Text("No, our cheat is designed so that if injection fails it will be untracable meaning you won't get");
			ImGui::Text("detected and banned.");
			ImGui::Text("   ");
			ImGui::Text("What are the detection rates?");
			ImGui::Text("There are no official detection rates as there are many different features but anything in the");
			ImGui::Text("normals tab will never get detected");
			ImGui::Text("   ");
			ImGui::Text("Is this a virus?");
			ImGui::Text("Put it simply no how ever while this exe is open we will monitor different things related to your");
			ImGui::Text("system and personal information. You can read the privacy policy found in this very tab for more info.");
			ImGui::Text("   ");
			ImGui::Text("Could I get unbanned if I get banned using this cheat?");
			ImGui::Text("The best answer is no but if you want a more detailed answer then allow me to give you just that.");
			ImGui::Text("RecRoom support for ban appeals is automated to give you the response of no. You won't get unbanned but");
			ImGui::Text("if your a creator or have made RecRoom Inc a lot of money they will make a human respond to your");
			ImGui::Text("appeal meaning then there is a chance of being unbanned so if your a creator making RecRoom Inc money");
			ImGui::Text("then yes you could be unbanned but if your just the average Joe then no you won't be unbanned.");
			ImGui::Text("   ");
			ImGui::Text("Can I contribute code to the cheat?");
			ImGui::Text("YES! We allow contributions to the cheat to improve the user expierance. If you want to contribute");
			ImGui::Text("to our project please contact the main coder listed in the Credits section. You will also be added");
			ImGui::Text("to the credits list if we decide to use your code. MAKE SURE YOUR CODE IS IN C++ OR LUA!!");
			ImGui::Text("   ");
			ImGui::Text("How long will support for this cheat last?");
			ImGui::Text("How ever long the main coder listed in the credits will continue it for.");
			ImGui::Text("   ");
			ImGui::Text("What is NameLock?");
			ImGui::Text("NameLock is a special type of ban RecRoom gives to people which have been caught ban evading or cheating.");
			ImGui::Text("NameLock only applies to PC users and is not enforced on any other devices besides PlayStation.");
			ImGui::Text("   ");
			ImGui::Text("Does Insanity offer a RecRoom spoofer to get around bans like NameLock?");
			ImGui::Text("No, we do not offer a temp or perm spoofer for NameLock but we to recommend a cleaner on GitHub.");
			ImGui::Text("Recommended cleaner: https://github.com/AlwaysAError/RecRoom-Spoofer");
			ImGui::Text("   ");
			ImGui::Text("What should I do if features are not working?");
			ImGui::Text("Please inform us of the feature which is not working in our Discord server.");
			ImGui::Text("   ");
			ImGui::Text("The cheat is launching but it's not working with RecRoom?");
			ImGui::Text("This can be because of a couple different factors. See the list below for help.");
			ImGui::Text("1. The exe has not been run as admin.");
			ImGui::Text("2. The bypass is out of date or is broken for some reason.");
			ImGui::Text("3. The cheat is not detecting the running process RecRoom.");
			ImGui::Text("4. Windows or a installed anti virus may be blocking the cheat from doing what is loves to do.");
			ImGui::Text("   ");
			ImGui::Text("Lua support");
			ImGui::Text("We support Lua scripts and have coded our cheat to support Lua scripts which are made to work with Cheat Engine.");
			ImGui::Text("   ");
			ImGui::Text("-------------------------------------------------------------------------------------------------------------------------------------------");
			ImGui::Text("   ");
			ImGui::Text("Privacy Policy");
			ImGui::Text("   ");
			ImGui::Text("Collected Data");
			ImGui::Text("We collect HWID's, file name's, installed apps, your advertisement ID, your Microsoft account email if your logged");
			ImGui::Text("into a Microsoft account and your Discord ID.");
			ImGui::Text("   ");
			ImGui::Text("What is done with my collected data?");
			ImGui::Text("Collected data is stored on a private server to get a better understanding of our users. Collected data is NOT tied");
			ImGui::Text("to any persons and we can not use this data against you. Collected data is not sold off to any companies or persons.");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar(); // End the tab bar
	}

	ImGui::End(); // End the window
}