//Button
//Change "I,m a button" to change the button name.

ImGui::Button("I'm a button");

//CheckBox
//Make sure to both of the items named is_checked if you want more than one checkbox.

static bool is_checked = false; // Persists across frames
ImGui::Checkbox("I'm a button", &is_checked);

//Text
//Allows for the adding of text!
ImGui::Text("   ");

//Slider
//Variables on the second line are the ones to edit the slider values. First variable is minimum and second variable is maximum.
//Change float slider to change the name of the slider.

static float value = 0.0f; // The variable to modify with the slider
ImGui::SliderFloat("Float Slider", &value, 0.0f, 100.0f, "%.1f");

//Select Menu
//Allow the user to select something from a drop down menu

    // List of items for the dropdown
static const char* items[] = { "Option 1", "Option 2", "Option 3", "Option 4" };
static int current_item = 0; // Index of the currently selected item

// Combo box
ImGui::Combo("Select Option", &current_item, items, IM_ARRAYSIZE(items));
// - "Select Option": Label for the combo box
// - &current_item: Pointer to the index of the selected item
// - items: Array of strings for the options
// - IM_ARRAYSIZE(items): Number of items in the array

// Display the selected item
ImGui::Text("Selected: %s", items[current_item]);

//Tabbing
//Allows for adding new tabs into the menu. Change // Tab 2 to the name of the tab so you know what tab your looking at+working with.

		// Tab 2
if (ImGui::BeginTabItem("Exploits Tab")) {
	ImGui::Text("You Are Now In The Exploits Tab");
	ImGui::EndTabItem();

//Text Input
//Allows someone to input text.

    static char buffer[256] = ""; // Buffer to store the input text (256 chars max)
    ImGui::InputText("Name", buffer, sizeof(buffer));
    // - "Name": Label for the input field
    // - buffer: Character array to store the input
    // - sizeof(buffer): Maximum buffer size

    ImGui::Text("You typed: %s", buffer); // Display the input

    //Sigma

//Separator
//Creates a line to separator stuff
    ImGui::Separator();