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

    file_panel left_panel("/home/limbo/COURSE_PROJECT/cmake-build-debug",
                          LINES - 1, COLS / 2, 0, 0);
    file_panel right_panel("/home/limbo/COURSE_PROJECT/cmake-build-debug",
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
                if (current_panel->get_content()[current_panel->get_current_ind()]
                            .content_type == CONTENT_TYPE::IS_DIR) {
                    current_panel->switch_directory(current_panel
                                                            ->get_content()[current_panel->get_current_ind()]
                                                            .name_content);
                }
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
            case KEY_F(3) : {
                create_functional_panel();
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