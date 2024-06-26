#include <iostream>
#include "file_panel.h"

int main() {
    setlocale(LC_ALL, "");
    initscr();
    noecho();
    cbreak();
    start_color();
    init_colors();
    keypad(stdscr, true);
    curs_set(0);

    bool flag_is_resize = false;


   /* file_panel left_panel("/home/limbo/COURSE_PROJECT/cmake-build-debug",
                          LINES - 1, COLS / 2, 0, 0);
    file_panel right_panel("/home/limbo/COURSE_PROJECT/cmake-build-debug",
                           LINES - 1, COLS / 2, 0, COLS / 2);*/

    file_panel left_panel(std::filesystem::current_path().string(),
                           LINES - 1, COLS / 2, 0, 0);
    file_panel right_panel(std::filesystem::current_path().string(),
                            LINES - 1, COLS / 2, 0, COLS / 2);

    file_panel *current_panel = &left_panel;
    current_panel->set_active_panel(true);

    left_panel.display_content();
    right_panel.display_content();

    int ch;

    while ((ch = getch()) != KEY_F(1)) {
        switch (ch) {
            case KEY_UP : {
                current_panel->move_cursor_and_pagination(KEY_UP);
                break;
            }
            case KEY_DOWN : {
                current_panel->move_cursor_and_pagination(KEY_DOWN);
                break;
            }
            case '\n' : {
                current_panel->switch_directory(current_panel
                                                        ->get_content()[current_panel->get_current_ind()]
                                                        .name_content);
                break;
            }
            case KEY_RESIZE : {
                flag_is_resize = true;
                clear();
                refresh();
                left_panel.resize_panel(LINES - 1, COLS / 2, 0, 0);
                right_panel.resize_panel(LINES - 1, COLS / 2, 0, COLS / 2);
                left_panel.display_content();
                right_panel.display_content();
                break;
            }
            case KEY_F(2) : {
                current_panel == &left_panel
                ? current_panel->delete_content(right_panel)
                : current_panel->delete_content(left_panel);
                break;
            }
            case KEY_F(3) : {
                current_panel == &left_panel
                ? current_panel->create_symlink(right_panel)
                : current_panel->create_symlink(left_panel);
                break;
            }
            case KEY_F(5) : {
                current_panel == &left_panel
                ? current_panel->create_directory(right_panel)
                : current_panel->create_directory(left_panel);
                break;
            }
            case KEY_F(6) : {
                current_panel == &left_panel
                ? current_panel->create_file(right_panel)
                : current_panel->create_file(left_panel);
                break;
            }
            case KEY_F(7) : {
                current_panel == &left_panel
                ? current_panel->rename_content(right_panel)
                : current_panel->rename_content(left_panel);
                break;
            }
            case KEY_F(8) : {
                current_panel == &left_panel
                ? current_panel->copy_content(right_panel)
                : current_panel->copy_content(left_panel);
                break;
            }
            case KEY_F(9) : {
                current_panel == &left_panel
                ? current_panel->move_content(right_panel)
                : current_panel->move_content(left_panel);
                break;
            }
            case 'p' : {
                current_panel == &left_panel
                ? current_panel->edit_permissions(right_panel)
                : current_panel->edit_permissions(left_panel);
                break;
            }
            case 'i' : {
                current_panel->analysis_selected_file();
                break;
            }
            case 'o' : {
                find_utility(left_panel, right_panel, current_panel->get_current_directory());
                break;
            }
            case 'f' : {
                filesystem_info_mount();
                break;
            }
            case 'v' : {
                current_panel->calculate_size();
                break;
            }
            case 'h' : {
                std::string return_result;
                create_history_panel(return_result);
                std::filesystem::path p(return_result);
                if (std::filesystem::exists(p)) {
                    if ((status(p).permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none
                    || (status(p).permissions() & std::filesystem::perms::owner_exec) == std::filesystem::perms::none) {
                        left_panel.display_content();
                        right_panel.display_content();
                        std::string message = "Cannot open directory: '" + p.filename().string() + "'";
                        create_error_panel(" Permission error ", message, HEIGHT_FUNCTIONAL_PANEL,
                                           WEIGHT_FUNCTIONAL_PANEL > message.length()
                                           ? WEIGHT_FUNCTIONAL_PANEL : message.length());
                        break;
                    }
                    current_panel->set_current_directory(return_result);
                    current_panel->read_current_dir();
                    current_panel->set_current_ind(0);
                    current_panel->set_start_ind(0);
                }
                break;
            }
            case 'm' : {
                char choice;
                create_help_menu(choice);
                break;
            }
            case '\t' : {
                current_panel->set_active_panel(false);
                current_panel == &left_panel ? (current_panel = &right_panel)
                                             : (current_panel = &left_panel);
                current_panel->set_active_panel(true);
                break;
            }
            default : {
                break;
            }
        }
        if (!flag_is_resize) {
            left_panel.display_content();
            right_panel.display_content();
        }
        flag_is_resize = false;
    }
    endwin();
    return 0;
}