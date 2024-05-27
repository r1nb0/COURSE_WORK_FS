#include <cstring>
#include "file_panel.h"

history_panel history_vec;
std::vector<std::pair<std::string, std::string>> help_vec{{"F2", "Deleting"}, {"F3", "Create symlink"}, {"F5", "Create dir"},
                                                     {"F6", "Create file"}, {"F7", "Rename content"}, {"F8", "Copy content"},
                                                     {"F9", "Move content"}, {"p", "Edit perms"}, {"h", "History"},
                                                     {"o", "Find utility"}, {"f", "Info mount"}, {"i", "Analyse file"},
                                                     {"v", "Calculate size"}};

void file_panel::read_current_dir() {
    if (!content.empty()) {
        content.clear();
    }
    struct stat st{};
    struct dirent *dir;
    DIR *d = opendir(this->current_directory.c_str());
    if (d == nullptr) {
        return;
    }
    while ((dir = readdir(d))) {
        std::string buffer(dir->d_name);
        if (buffer == ".") {
            continue;
        }
        std::string full_path = current_directory + "/" + buffer;
        lstat(full_path.c_str(), &st);
        ssize_t size = st.st_size;
        char date[25];
        std::strftime(date, sizeof(date), "%D %H:%M", std::gmtime(&st.st_mtim.tv_sec));
        CONTENT_TYPE content_type;
        COLOR_INDEX color_index;
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            content_type = CONTENT_TYPE::IS_DIR;
            color_index = MAGENTA_COLOR;
        } else if ((st.st_mode & S_IFMT) == S_IFLNK) {
            std::filesystem::path link_p(full_path);
            std::filesystem::path link_target = std::filesystem::read_symlink(link_p);
            if (!std::filesystem::exists(link_target)) {
                content_type = CONTENT_TYPE::IS_HANGING_LINK;
                color_index = RED_COLOR;
            }
            else if (is_symlink(link_p) && is_directory(link_p)) {
                content_type = CONTENT_TYPE::IS_LNK_TO_DIR;
                color_index = MAGENTA_COLOR;
            } else {
                content_type = CONTENT_TYPE::IS_LNK;
                color_index = WHITE_COLOR;
            }
        } else if ((st.st_mode & S_IFMT) == S_IFREG) {
            content_type = CONTENT_TYPE::IS_REG;
            std::filesystem::path reg_path(full_path);
            std::filesystem::path p = reg_path.extension();
            if (p == ".tmp") {
                color_index = BLUE_COLOR;
            } else if (p == ".txt") {
                color_index = YELLOW_COLOR;
            } else if (p == ".c" || p == ".cpp" || p == ".h" || p == ".hpp") {
                color_index = GREEN_COLOR;
            } else color_index = WHITE_COLOR;
         }
        content.emplace_back(buffer, date, size, content_type, color_index);
    }
    std::sort(content.begin(), content.end(),
              [](const info &first, const info &second) {
                  if (first.content_type != second.content_type) {
                      return first.content_type < second.content_type;
                  }
                  return first.name_content < second.name_content;
              });
}

file_panel::file_panel(std::string_view _current_directory, size_t _rows, size_t _cols, size_t _x, size_t _y) {
    this->current_directory = _current_directory;
    this->current_ind = 0;
    this->start_index = 0;
    this->active_panel = false;
    this->win = newwin(static_cast<int>(_rows), static_cast<int>(_cols),
                       static_cast<int>(_x), static_cast<int>(_y));
    this->panel = new_panel(win);
    keypad(this->win, true);
    read_current_dir();
    update_panels();
    doupdate();
}

void file_panel::display_lines() {
    if (active_panel) {
        wattron(win, COLOR_PAIR(1));
    }
    for (size_t i = 1; i < LINES - 2; i++) {
        mvwaddch(win, i, COLS / 2 - DATE_LEN, ACS_VLINE);
        mvwaddch(win, i, COLS / 2 - DATE_LEN - MAX_SIZE_LEN, ACS_VLINE);
    }
    wattroff(win, COLOR_PAIR(1));
}

void file_panel::display_content() {
    if (COLS < 66) {
        std::string message = "Terminal too narrow to show contents";
        create_error_panel(" Window size error ", message,
                           HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);

        endwin();
        exit(-1);
    }
    werase(win);
    refresh();
    display_box();
    display_lines();
    display_headers();
    display_current_dir();
    size_t ind_offset = 2;
    for (size_t i = this->start_index; i < content.size() && i < LINES - 4 + this->start_index; i++) {
        if (active_panel) {
            attron(A_BOLD | COLOR_PAIR(10));
            mvprintw(LINES - 1, 0, "%*s", COLS, " ");
            int weight_line = COLS - 29;
            std::string current_path = current_directory + "/" + content[current_ind].name_content;
            std::string final_str;
            if (current_path.length() > weight_line) {
                convert_str_to_window_size(final_str, current_path, weight_line);
            } else {
                final_str = current_path;
            }
            mvprintw(LINES - 1, 0, "%s%zu%s%zu%s%s", "File:     ",
                     current_ind + 1, " of ", content.size(), "     Path: ",
                     final_str.c_str());
            attroff(A_BOLD | COLOR_PAIR(10));
        }
        if (i == current_ind && active_panel) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, static_cast<int>(ind_offset), 1, "%*s", (COLS / 2) - 2, " ");

        if (((current_ind != ind_offset - 2 + start_index) || !active_panel)) {
            wattron(win, COLOR_PAIR(content[i].color_index));
        }
        std::string output_string = content[i].name_content;
        convert_to_output(output_string, content[i].content_type);
        size_t len_line = COLS / 2 - DATE_LEN - MAX_SIZE_LEN - 1;
        //? maybe create error if panel resize < 5
        if (len_line < output_string.length()) {
            std::string short_str;
            convert_str_to_window_size(short_str, output_string, len_line);
            mvwprintw(win, static_cast<int>(ind_offset), 1, "%s", short_str.c_str());
        } else {
            mvwprintw(win, static_cast<int>(ind_offset), 1, "%s",
                      output_string.c_str());
        }

        wattroff(win, COLOR_PAIR(3));

        if (active_panel && ind_offset - 2 + start_index != current_ind) {
            wattron(win, COLOR_PAIR(1));
        }

        mvwaddch(win, ind_offset, COLS / 2 - DATE_LEN, ACS_VLINE);
        mvwaddch(win, ind_offset, COLS / 2 - DATE_LEN, ACS_VLINE);
        mvwaddch(win, ind_offset, COLS / 2 - DATE_LEN - MAX_SIZE_LEN, ACS_VLINE);

        wattroff(win, COLOR_PAIR(1));

        std::string size_str = std::to_string(content[i].size_content);


        mvwprintw(win, static_cast<int>(ind_offset),
                  (COLS / 2) - DATE_LEN - static_cast<int>(size_str.length()),
                  "%s", size_str.c_str());

        mvwprintw(win, static_cast<int>(ind_offset),
                  (COLS / 2) - DATE_LEN + 1,
                  "%s", content[i].last_redact_content.c_str());

        wattroff(win, A_REVERSE);

        ind_offset++;
    }
    refresh_panels();
}

void file_panel::resize_panel(size_t _rows, size_t _cols, size_t _x, size_t _y) {
    start_index = static_cast<int>(current_ind / (LINES - 4)) * (LINES - 4);
    werase(win);
    wresize(win, static_cast<int>(_rows), static_cast<int>(_cols));
    mvwin(win, static_cast<int>(_x), static_cast<int>(_y));
    wrefresh(win);
    refresh();
}

void file_panel::refresh_panels() {
    wrefresh(win);
    refresh();
}

PANEL *file_panel::get_panel() const {
    return panel;
}

void file_panel::display_headers() {
    wattron(win, A_BOLD);
    mvwprintw(win, 1, (((COLS / 2) - DATE_LEN - MAX_SIZE_LEN) / 2) - 1, "%s", HEADER_NAME);
    mvwprintw(win, 1, (COLS / 2) - DATE_LEN - MAX_SIZE_LEN + 4, "%s", HEADER_SIZE);
    mvwprintw(win, 1, (COLS / 2) - DATE_LEN + 3, "%s", HEADER_MODIFY_DATE);
    wattroff(win, A_BOLD);
}

void file_panel::switch_directory(const std::string &_direction) {
    if (content[current_ind].content_type == CONTENT_TYPE::IS_LNK_TO_DIR
    || content[current_ind].content_type == CONTENT_TYPE::IS_DIR) {
        DIR *d;
        std::string new_current_directory;
        if (_direction == "..") {
            if (current_directory == "/")
                return;
            new_current_directory = current_directory.substr(0, current_directory.find_last_of('/'));
            if (new_current_directory.empty()) {
                new_current_directory = "/";
            }
            d = opendir(new_current_directory.c_str());
        } else {
            if (current_directory == "/") {
                new_current_directory = current_directory + _direction;
            } else {
                new_current_directory = current_directory + "/" + _direction;
            }
            d = opendir(new_current_directory.c_str());
        }
        std::filesystem::path check_perm(new_current_directory);

        if ((std::filesystem::status(check_perm).permissions() & std::filesystem::perms::owner_read) ==
            std::filesystem::perms::none
            || (std::filesystem::status(check_perm).permissions() & std::filesystem::perms::owner_exec) ==
               std::filesystem::perms::none) {
            std::string message = "Cannot chdir to " + _direction;
            create_error_panel(" Permission error ",
                               message, 8,
                               WEIGHT_FUNCTIONAL_PANEL > message.length()
                               ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
            return;
        }
        if (d == nullptr) {
            return;
        }
        start_index = 0;
        current_ind = 0;
        auto it = std::find(history_vec.history_path.begin(),
                            history_vec.history_path.end(),
                            new_current_directory);
        if (it == history_vec.history_path.end()) {
            if (new_current_directory.length() > history_vec.max_len) {
                history_vec.max_len = new_current_directory.length();
            }
            history_vec.history_path.push_back(new_current_directory);
        }
        current_directory = new_current_directory;
        read_current_dir();
    } else {
        std::string command = "xdg-open " + current_directory + "/" + _direction + " 2>/dev/null";
        if((system(command.c_str())) == -1) {
            return;
        }
    }
}

file_panel::~file_panel() {
    del_panel(panel);
    delwin(win);
    content.clear();
}

void file_panel::move_cursor_and_pagination(size_t _direction) {
    if (_direction == KEY_UP) {
        if (current_ind == 0) {
            this->start_index = 0;
        } else if (current_ind != start_index) {
            current_ind--;
        } else {
            start_index -= LINES - 4;
            current_ind--;
        }
    } else if (_direction == KEY_DOWN) {
        if (current_ind + 1 < content.size()) {
            if (current_ind + 1 >= LINES - 4 + start_index) {
                start_index += LINES - 4;
            }
            current_ind++;
        }
    }
}

const std::vector<info> &file_panel::get_content() const {
    return content;
}

size_t file_panel::get_current_ind() const {
    return current_ind;
}

void file_panel::set_active_panel(bool _active_panel) {
    active_panel = _active_panel;
}

void file_panel::display_current_dir() {
    if (active_panel) {
        wattron(win, COLOR_PAIR(2));
        wattron(win, A_BOLD);
    }
    std::string short_dir;
    size_t len = current_directory.length();
    int width_window = COLS / 2 - 3;
    if (len > width_window) {
        convert_str_to_window_size(short_dir, current_directory, width_window);
        mvwprintw(win, 0, 2, "%s", short_dir.c_str());
    } else {
        mvwprintw(win, 0, 2, "%s", current_directory.c_str());
    }
    wattroff(win, COLOR_PAIR(2));
    wattroff(win, A_BOLD);
}

void file_panel::display_box() {
    if (active_panel) {
        wattron(win, COLOR_PAIR(1));
    }
    box(win, 0, 0);
    wattroff(win, COLOR_PAIR(2));
}

void file_panel::rename_content(file_panel &_other_panel) {
    std::string new_name = content[current_ind].name_content;
    bool flag_entry;
    if (new_name != "/..") {
        flag_entry = create_redact_other_func_panel(HEADER_RENAME, "Rename '"
                                                                   + new_name + "' to:", new_name,
                                                    HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);
        if (new_name == content[current_ind].name_content) {
            return;
        }
        if (flag_entry) {
            std::filesystem::path dir_path(current_directory);
            if ((std::filesystem::status(dir_path).permissions() & std::filesystem::perms::owner_write) ==
                std::filesystem::perms::none) {
                display_content();
                _other_panel.display_content();
                std::string message =
                        "Cannot rename file/dir :: '" + content[current_ind].name_content + "' to '" + new_name + "'";
                create_error_panel(" Permission error ", message,
                                   HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
                return;
            }
            if (exists(dir_path)) {
                if (!exists(dir_path / new_name)) {
                    try {
                        std::filesystem::rename(dir_path / content[current_ind].name_content, dir_path / new_name);
                    } catch (std::filesystem::filesystem_error& e) {
                        display_content();
                        _other_panel.display_content();
                        int error_ind = e.code().value();
                        if (error_ind == 13) {
                            generate_permission_error(e);
                        } else if (error_ind == 20 || error_ind == 21 || error_ind == 22) {
                            generate_incompatible_error(e);
                        }
                    }
                    read_current_dir();
                    if (_other_panel.current_directory == current_directory) {
                        _other_panel.read_current_dir();
                    }
                } else {
                    display_content();
                    _other_panel.display_content();
                    std::string message = "File with name :: '" + new_name + "' is already exists.";
                    create_error_panel(" Rename error ", message,
                                       HEIGHT_FUNCTIONAL_PANEL - 2,
                                       WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                       message.length() + 2);
                }
            }
        }
    }
}

void file_panel::create_directory(file_panel &_other_panel) {
    std::string name_directory;
    bool entry_flag = create_redact_other_func_panel(HEADER_CREATE_DIR,
                                                     DESCRIPTION_DIRECTORY,
                                                     name_directory, HEIGHT_FUNCTIONAL_PANEL,
                                                     WEIGHT_FUNCTIONAL_PANEL);
    if (entry_flag) {
        std::filesystem::path dir_path(current_directory);
        if ((std::filesystem::status(dir_path).permissions() & std::filesystem::perms::owner_write) ==
            std::filesystem::perms::none) {
            display_content();
            _other_panel.display_content();
            std::string message = "Cannot create directory :: '" + name_directory + "'";
            create_error_panel(" Permission error ", message,
                               HEIGHT_FUNCTIONAL_PANEL - 2,
                               WEIGHT_FUNCTIONAL_PANEL > message.length()
                               ? WEIGHT_FUNCTIONAL_PANEL : message.length() +  2);
            return;
        }
        if (exists(dir_path)) {
            if (!exists(dir_path / name_directory)) {
                std::filesystem::create_directory(dir_path / name_directory);
                if (_other_panel.current_directory == this->current_directory) {
                    _other_panel.read_current_dir();
                }
                read_current_dir();
                size_t i = 0;
                const auto& vec = this->get_content();
                for (auto && it : vec) {
                    if (it.name_content == name_directory) {
                        this->set_current_ind(i);
                        this->set_start_ind(static_cast<int>(i / (LINES - 4)) * (LINES - 4));
                        break;
                    }
                    i++;
                }
            } else {
                display_content();
                _other_panel.display_content();
                std::string message = "File with name :: '" + name_directory + "' is already exists.";
                create_error_panel(" Directory error ", message, HEIGHT_FUNCTIONAL_PANEL,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
            }
        }
    }
}

void file_panel::create_file(file_panel &_other_panel) {
    std::string name_file;
    bool entry_flag = create_redact_other_func_panel(HEADER_CREATE_FILE,
                                                     DESCRIPTION_FILE,
                                                     name_file,
                                                     HEIGHT_FUNCTIONAL_PANEL,
                                                     WEIGHT_FUNCTIONAL_PANEL);
    if (entry_flag) {
        std::filesystem::path dir_path(current_directory);
        if ((std::filesystem::status(dir_path).permissions() & std::filesystem::perms::owner_write) ==
            std::filesystem::perms::none) {
            display_content();
            _other_panel.display_content();
            std::string message = "Cannot create file :: '" + name_file + "'";
            create_error_panel(" Permission error ", message,
                               HEIGHT_FUNCTIONAL_PANEL - 2,
                               WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
            return;
        }
        if (exists(dir_path)) {
            if (!exists(dir_path / name_file)) {
                std::ofstream newFile(dir_path / name_file);
                newFile.close();
                if (_other_panel.current_directory == this->current_directory) {
                    _other_panel.read_current_dir();
                }
                read_current_dir();
                size_t i = 0;
                const auto& vec = this->get_content();
                for (auto && it : vec) {
                    if (it.name_content == name_file) {
                        this->set_current_ind(i);
                        this->set_start_ind(static_cast<int>(i / (LINES - 4)) * (LINES - 4));
                        break;
                    }
                    i++;
                }
            } else {
                display_content();
                _other_panel.display_content();
                std::string message = "File with name :: '" + name_file + "' is already exists.";
                create_error_panel(" File error ", message, HEIGHT_FUNCTIONAL_PANEL,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
            }
        }
    }

}

void file_panel::create_symlink(file_panel &_other_panel) {
    std::string namelink;
    std::string pointing_to = _other_panel.content[_other_panel.current_ind].name_content;
    bool entry_flag = symlink_hardlink_func_panel(HEADER_CREATE_SYMLINK, namelink, pointing_to,
                                                  HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);

    if (entry_flag) {
        std::filesystem::path dir_path(current_directory);
        std::filesystem::path other_panel_path(_other_panel.current_directory);
        if ((std::filesystem::status(dir_path).permissions() & std::filesystem::perms::owner_write) ==
            std::filesystem::perms::none) {
            display_content();
            _other_panel.display_content();
            std::string message = "Cannot create symlink : '" + namelink + "' -> '" + pointing_to + "'";
            create_error_panel("Permission error", message,
                               HEIGHT_FUNCTIONAL_PANEL - 2,
                               WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL : message.length() +
                                                                                                      2);
            return;
        }
        if (exists(dir_path)) {
            if (!exists(dir_path / namelink) && exists(other_panel_path / pointing_to)) {
                if (is_directory((other_panel_path / pointing_to))) {
                    std::filesystem::create_directory_symlink(other_panel_path / pointing_to, dir_path / namelink);
                } else {
                    std::filesystem::create_symlink(other_panel_path / pointing_to, dir_path / namelink);
                }
                if (_other_panel.current_directory == this->current_directory) {
                    _other_panel.read_current_dir();
                }
                size_t i = 0;
                const std::vector<info>& vec = this->get_content();
                this->read_current_dir();
                for (auto && it : vec) {
                    if (it.name_content == namelink) {
                        this->set_current_ind(i);
                        this->set_start_ind(static_cast<int>(i / (LINES - 4)) * (LINES - 4));
                        break;
                    }
                    i++;
                }
            }
        }
    }

}

void file_panel::copy_content(file_panel &_other_panel) {
    if (content[current_ind].name_content != "/..") {
        std::string path = _other_panel.current_directory;
        bool entry_flag = create_redact_other_func_panel(HEADER_COPY, "Copy '" + content[current_ind]
                                                                 .name_content + "' to::", path,
                                                         HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);
        if (entry_flag) {
            std::filesystem::path copy_path_from(current_directory + "/" + content[current_ind].name_content);
            std::filesystem::path copy_path_to(path);
            std::filesystem::path copy_to_full(copy_path_to / content[current_ind].name_content);
            if (!exists(copy_path_to)) {
                display_content();
                _other_panel.display_content();
                std::string message = "Cannot copy content cause : '" + path + "' does not exist";
                create_error_panel(" Copy error ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
                return;
            }

            if (std::filesystem::equivalent(current_directory, copy_path_to)) {
                display_content();
                _other_panel.display_content();
                std::string message = "Can't copy content, paths are the same";
                create_error_panel(" Copy error ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
                return;
            }

            std::string full_from = copy_path_from.string();
            if (full_from.length() <= path.length()) {
                std::string substr = path.substr(0, full_from.length());
                if (substr == full_from) {
                    display_content();
                    _other_panel.display_content();
                    std::string message = "Can't copy content, FROM path consist TO path";
                    create_error_panel(" Copy recursive error ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                       WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                       message.length() + 2);
                    return;
                }
            }

            //+is_dir
            if ((is_directory(copy_path_from)) && ((std::filesystem::status(copy_path_from).permissions() & std::filesystem::perms::owner_read) ==
                std::filesystem::perms::none
                || (std::filesystem::status(copy_path_from).permissions() & std::filesystem::perms::owner_exec) ==
                   std::filesystem::perms::none)) {
                display_content();
                _other_panel.display_content();
                std::string message = "Cannot copy from '" + copy_path_from.string() + "'";
                create_error_panel(" Permission error ",
                                   message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length()
                                   ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                return;
            }


            if ((std::filesystem::status(copy_path_to).permissions() & std::filesystem::perms::owner_write)
                == std::filesystem::perms::none) {
                display_content();
                _other_panel.display_content();
                std::string message = "Cannot copy to '" + copy_path_to.string() + "'";
                create_error_panel(" Permission error ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length()
                                   ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                return;
            }

            if (is_directory(copy_path_from)) {
                if (!exists(copy_to_full)) {
                        //try catch exist
                    if (content[current_ind].content_type == CONTENT_TYPE::IS_LNK_TO_DIR) {
                        std::filesystem::copy_symlink(copy_path_from, copy_path_to / content[current_ind].name_content);
                    } else {
                        overwrite_content_copy(_other_panel, copy_path_from, copy_path_to, false);
                    }
                    if (path == _other_panel.current_directory) {
                        _other_panel.read_current_dir();
                    }
                    return;
                } else {
                    if (!is_directory((copy_to_full))
                    || is_symlink(copy_to_full)) {
                        display_content();
                        _other_panel.display_content();
                        std::string message = (copy_path_to / content[current_ind].name_content).string();
                        create_error_panel(" Incompatible types ", message,
                                           HEIGHT_FUNCTIONAL_PANEL - 2,
                                           WEIGHT_FUNCTIONAL_PANEL > message.length()
                                           ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                        return;
                    }
                    std::string message = "Overwrite: " + copy_to_full.string();
                    display_content();
                    _other_panel.display_content();
                    REMOVE_TYPE type = create_remove_panel(" Copy file(s) ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                                           WEIGHT_FUNCTIONAL_PANEL > message.length()
                                                           ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                    if (is_symlink(copy_path_from)
                        && (type == REMOVE_TYPE::REMOVE_ALL || type == REMOVE_TYPE::REMOVE_THIS)) {
                        std::filesystem::remove(copy_to_full);
                        std::filesystem::copy_symlink(copy_path_from, copy_to_full);
                        _other_panel.read_current_dir();
                        return;
                    }
                    if (type == REMOVE_TYPE::REMOVE_ALL) {
                        overwrite_content_copy(_other_panel, copy_path_from, copy_path_to, true);
                    } else if (type == REMOVE_TYPE::SKIP || type == REMOVE_TYPE::STOP_REMOVE) {
                        return;
                    } else {
                        overwrite_content_copy(_other_panel, copy_path_from, copy_path_to, false);
                    }
                }
                return;
            }
            try {
                if (std::filesystem::exists(copy_to_full)) {
                    bool is_correct_types = true;
                    if (is_symlink(copy_path_from)) {
                        if (!is_symlink(copy_to_full)) {
                            is_correct_types = false;
                        }
                    } else if (!is_symlink(copy_path_from)) {
                        if (is_symlink(copy_to_full)) {
                            is_correct_types = false;
                        }
                    }
                    if (!is_correct_types) {
                        throw std::filesystem::filesystem_error("Another types", copy_path_from,
                                                                copy_to_full,
                                                                std::error_code(21, std::iostream_category()));
                    }
                }
                if (is_symlink(copy_path_from)) {
                    if (exists(copy_to_full)) {
                        std::filesystem::remove(copy_to_full);
                        std::filesystem::copy_symlink(copy_path_from, copy_path_to / content[current_ind].name_content);
                        if (copy_path_to == _other_panel.current_directory) {
                            _other_panel.read_current_dir();
                        }
                    }
                    std::filesystem::copy_symlink(copy_path_from, copy_path_to / content[current_ind].name_content);
                    _other_panel.read_current_dir();
                } else if (is_character_file(copy_path_from)
                           || is_regular_file(copy_path_from)
                           || is_block_file(copy_path_from)
                           || is_fifo(copy_path_from)
                           || is_socket(copy_path_from)) {
                    if (!exists(copy_to_full)) {
                        std::filesystem::copy_file(copy_path_from, copy_to_full);
                    } else {
                        display_content();
                        _other_panel.display_content();
                        std::string message = "Overwrite: " + copy_to_full.string();
                        REMOVE_TYPE type = create_remove_panel(" Copy file(s) ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                                               WEIGHT_FUNCTIONAL_PANEL > message.length()
                                                               ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                        if (type == REMOVE_TYPE::REMOVE_ALL || type == REMOVE_TYPE::REMOVE_THIS) {
                            std::filesystem::copy_file(copy_path_from, copy_to_full,
                                                       std::filesystem::copy_options::overwrite_existing);
                        }
                    }
                    if (copy_path_to == _other_panel.current_directory) {
                        _other_panel.read_current_dir();
                    }
                }
            } catch (std::filesystem::filesystem_error& e) {
                display_content();
                _other_panel.display_content();
                int error_ind = e.code().value();
                if (error_ind == 13) {
                    generate_permission_error(e);
                } else if (error_ind == 21 || error_ind == 22) {
                    generate_incompatible_error(e);
                }
            }
        }
    }
}

void file_panel::overwrite_content_copy(file_panel &_other_panel, std::filesystem::path &_from, std::filesystem::path &_to, bool _all) {
    bool overwrite_other = _all;
    REMOVE_TYPE type;
    std::stack<std::filesystem::path> dir_stack;
    dir_stack.push(_from);
    if (!exists((_to / content[current_ind].name_content))) {
        std::filesystem::create_directory(_to / content[current_ind].name_content);
    }
    size_t index = _from.string().rfind(content[current_ind].name_content);
    while (!dir_stack.empty()) {
        std::filesystem::path current_path = dir_stack.top();
        dir_stack.pop();
        for (const auto &entry: std::filesystem::directory_iterator(current_path, std::filesystem::directory_options::skip_permission_denied)) {
            std::string path_string = entry.path().string();
            std::filesystem::path copy_part(path_string.substr(index));
            bool flag_skip = false;
            std::filesystem::path full_copy_path = _to / copy_part;
            try {
                if (!exists(full_copy_path)) {
                    if (entry.is_directory()) {
                        if (entry.is_symlink()) {
                            std::filesystem::copy_symlink(entry.path(), full_copy_path);
                        } else {
                            std::filesystem::create_directory(full_copy_path);
                        }
                    } else if (entry.is_symlink()) {
                        std::filesystem::copy_symlink(entry.path(), full_copy_path);
                    } else if (entry.is_regular_file() || entry.is_character_file() || entry.is_block_file()
                               || entry.is_socket() || entry.is_fifo()) {
                        std::filesystem::copy_file(entry.path(), full_copy_path);
                    }
                } else {
                    if (!overwrite_other) {
                        display_content();
                        _other_panel.display_content();
                        std::string message = "Overwrite: " + path_string;
                        type = create_remove_panel(HEADER_COPY, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                                   WEIGHT_FUNCTIONAL_PANEL > message.length()
                                                   ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                    }
                    if (type == REMOVE_TYPE::REMOVE_ALL) {
                        display_content();
                        _other_panel.display_content();
                        overwrite_other = true;
                    } else if (type == REMOVE_TYPE::STOP_REMOVE) {
                        return;
                    } else if (type == REMOVE_TYPE::REMOVE_THIS || overwrite_other) {
                        bool is_correct_types = true;
                        if ((entry.is_directory() && !entry.is_symlink())) {
                            if (is_directory(full_copy_path) && !is_symlink(full_copy_path)) {
                                is_correct_types = true;
                            } else {
                                is_correct_types = false;
                                flag_skip = true;
                            }
                        } else if ((entry.is_symlink() && entry.is_directory()) ||
                                   (entry.is_symlink() && !entry.is_directory())) {
                            if (!is_symlink(full_copy_path)) {
                                is_correct_types = false;
                            }
                        } else if (!entry.is_symlink()) {
                            if (is_symlink(full_copy_path)) {
                                is_correct_types = false;
                            }
                        }
                        if (is_correct_types) {
                            if ((entry.is_directory() && entry.is_symlink()) || (entry.is_symlink())) {
                                std::filesystem::remove(full_copy_path);
                                std::filesystem::copy_symlink(entry.path(), full_copy_path);
                            } else if (entry.is_regular_file() || entry.is_fifo() || entry.is_character_file()
                                       || entry.is_block_file() || entry.is_socket()) {
                                std::filesystem::copy_file(entry.path(), full_copy_path,
                                                           std::filesystem::copy_options::overwrite_existing);
                            }
                        } else {
                            throw std::filesystem::filesystem_error("Another types", entry.path(), full_copy_path,
                                                                    std::error_code(21, std::iostream_category()));
                        }
                    }
                }
            } catch (std::filesystem::filesystem_error& e) {
                display_content();
                _other_panel.display_content();
                int error_ind = e.code().value();
                if (error_ind == 13) {
                    generate_permission_error(e);
                } else if (error_ind == 21 || error_ind == 22) {
                    generate_incompatible_error(e);
                }
            }
            if (!flag_skip && entry.is_directory()) {
                dir_stack.push(entry.path());
            }
        }
    }
}

void file_panel::move_content(file_panel& _other_panel) {
    if (content[current_ind].name_content != "/..") {
        std::string path_to_move = _other_panel.current_directory;
        bool entry_flag = create_redact_other_func_panel(HEADER_MOVE, "Move '" + content[current_ind]
                                                                 .name_content + "' to::", path_to_move,
                                                         HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);
        if (entry_flag) {
            std::filesystem::path move_from(current_directory + "/" + content[current_ind].name_content);
            std::filesystem::path move_to(path_to_move);
            std::filesystem::path move_to_full(move_to / content[current_ind].name_content);

            if (!exists(move_to)) {
                display_content();
                _other_panel.display_content();
                std::string message = "Cannot copy content cause : '" + path_to_move + "' does not exist";
                create_error_panel(HEADER_MOVE, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
                return;
            }

            if (std::filesystem::equivalent(current_directory, move_to)) {
                display_content();
                _other_panel.display_content();
                std::string message = "Can't move content, paths are the same";
                create_error_panel(" Move error ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                   message.length() + 2);
                return;
            }

            std::string full_from = move_from.string();
            if (full_from.length() <= path_to_move.length()) {
                std::string substr = path_to_move.substr(0, full_from.length());
                if (substr == full_from) {
                    display_content();
                    _other_panel.display_content();
                    std::string message = "Can't copy content, FROM path consist TO path";
                    create_error_panel(" Copy recursive error ", message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                       WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                       message.length() + 2);
                    return;
                }
            }

            //after deleting last -> cursor > content.size()
            if (!exists(move_to_full)) {
                try {
                    std::filesystem::rename(move_from, move_to_full);
                    _other_panel.read_current_dir();
                    this->read_current_dir();
                    if (current_ind >= content.size()) {
                        current_ind = content.size() - 1;
                    }
                } catch (std::filesystem::filesystem_error& e) {
                    display_content();
                    _other_panel.display_content();
                    int error_ind = e.code().value();
                    if (error_ind == 13) {
                        generate_permission_error(e);
                    } else if (error_ind == 21 || error_ind == 22) {
                        generate_incompatible_error(e);
                    }
                    return;
                }
            } else {
                display_content();
                _other_panel.display_content();
                if (is_directory(move_from)) {
                    if (!is_directory(move_to_full) || is_symlink(move_to_full)) {
                        display_content();
                        _other_panel.display_content();
                        std::string message = move_to_full.string();
                        create_error_panel(" Incompatible types ", message,
                                           HEIGHT_FUNCTIONAL_PANEL - 2,
                                           WEIGHT_FUNCTIONAL_PANEL > message.length()
                                           ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                        return;
                    }
                    if ((status(move_from).permissions() & std::filesystem::perms::owner_read)
                        == std::filesystem::perms::none) {
                        display_content();
                        _other_panel.display_content();
                        std::string message = "Cannot move from '" + move_from.string() + "'";
                        create_error_panel(" Permission error ",
                                           message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                           WEIGHT_FUNCTIONAL_PANEL > message.length()
                                           ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                        return;
                    }
                    overwrite_content_move(_other_panel, move_from, move_to);
                    if (std::filesystem::exists(move_from) && std::filesystem::is_empty(move_from)) {
                        std::filesystem::remove(move_from);
                    }
                    this->read_current_dir();
                    if (current_ind >= content.size()) {
                        current_ind = content.size() - 1;
                    }
                } else {
                    std::string message = "Overwrite: " + move_to_full.string();
                    REMOVE_TYPE type = create_remove_panel(HEADER_MOVE, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                                           WEIGHT_FUNCTIONAL_PANEL > message.length()
                                                           ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                    if (type == REMOVE_TYPE::REMOVE_ALL || type == REMOVE_TYPE::REMOVE_THIS) {
                        try {
                            if (std::filesystem::exists(move_to_full)) {
                                bool is_correct_types = true;
                                if (is_symlink(move_from)) {
                                    if (!is_symlink(move_to_full)) {
                                        is_correct_types = false;
                                    }
                                } else if (!is_symlink(move_from)) {
                                    if (is_symlink(move_to_full)) {
                                        is_correct_types = false;
                                    }
                                }
                                if (!is_correct_types) {
                                    throw std::filesystem::filesystem_error("Another types", move_from,
                                                                            move_to_full, std::error_code(21,
                                                                                                          std::iostream_category()));
                                }
                            }
                            std::filesystem::remove(move_to_full);
                            std::filesystem::rename(move_from, move_to_full);
                            read_current_dir();
                            if (current_ind >= content.size()) {
                                current_ind = content.size() - 1;
                            }
                            if (_other_panel.current_directory == path_to_move) {
                                _other_panel.read_current_dir();
                            }
                        } catch (std::filesystem::filesystem_error& e) {
                            display_content();
                            _other_panel.display_content();
                            int error_ind = e.code().value();
                            if (error_ind == 13) {
                                generate_permission_error(e);
                            } else if (error_ind == 21 || error_ind == 22) {
                                generate_incompatible_error(e);
                            }
                        }
                    } else {
                        return;
                    }
                }
            }
        }
    }
}

const std::string &file_panel::get_current_directory() const {
    return current_directory;
}

void file_panel::edit_permissions(file_panel &_other_panel) {
    if (content[current_ind].name_content == "..") {
        return;
    }
    std::string path = current_directory + "/" + content[current_ind].name_content;
    std::filesystem::path dir_path(path);
    std::filesystem::perms p = std::filesystem::status(dir_path).permissions();
    char_permissions perms;
    auto parse_arg = [=](std::string &_buffer, char op, std::filesystem::perms perms) {
        std::filesystem::perms::none == (perms & p) ? _buffer.push_back('-') : _buffer.push_back(op);
    };

    parse_arg(perms.owner_perm, 'r', std::filesystem::perms::owner_read);
    parse_arg(perms.owner_perm, 'w', std::filesystem::perms::owner_write);
    parse_arg(perms.owner_perm, 'x', std::filesystem::perms::owner_exec);
    parse_arg(perms.group_perm, 'r', std::filesystem::perms::group_read);
    parse_arg(perms.group_perm, 'w', std::filesystem::perms::group_write);
    parse_arg(perms.group_perm, 'x', std::filesystem::perms::group_exec);
    parse_arg(perms.other_perm, 'r', std::filesystem::perms::others_read);
    parse_arg(perms.other_perm, 'w', std::filesystem::perms::others_write);
    parse_arg(perms.other_perm, 'x', std::filesystem::perms::others_exec);

    perms.recursive = '-';

    bool entry_flag = change_permissions_panel(EDIT_PERMISSIONS, content[current_ind].name_content,
                                               HEIGHT_FUNCTIONAL_PANEL + 1,
                                               WEIGHT_FUNCTIONAL_PANEL,
                                               perms);

    if (entry_flag) {
        std::filesystem::perms new_perms = std::filesystem::perms::none;
        fill_permissions(perms, new_perms);
        try {
            if (perms.recursive == 'X' && std::filesystem::is_directory(path)) {
                for (auto &&entry: std::filesystem::recursive_directory_iterator(path)) {
                    std::filesystem::permissions(entry.path(), new_perms, std::filesystem::perm_options::replace);
                }
            }
            std::filesystem::permissions(path, new_perms, std::filesystem::perm_options::replace);
        } catch (std::filesystem::filesystem_error &e) {
            int error_ind = e.code().value();
            if (error_ind == 13) {
                display_content();
                _other_panel.display_content();
                generate_permission_error(e);
            }
        }
    }
}

void file_panel::delete_content(file_panel &_other_panel) {
    if (content[current_ind].name_content == "..") {
        return;
    }
    std::string current_path = current_directory + "/" + content[current_ind].name_content;
    std::filesystem::path p(current_path);
    REMOVE_TYPE type;
    bool flag_permission_read = true;
    if (is_directory(p) && !is_symlink(p)) {
        try {
            flag_permission_read = std::filesystem::is_empty(p);
        } catch (std::filesystem::filesystem_error &e) {
            int error_ind = e.code().value();
            if (error_ind == 13) {
                generate_permission_error(e);
            }
            return;
        }
    }
    if ((is_directory(p) && !is_symlink(p)) && !flag_permission_read) {
        sequential_removing(p, _other_panel, false);
        if (!std::filesystem::exists(_other_panel.current_directory + "/" + _other_panel
                .content[_other_panel.current_ind].name_content)) {
            if (_other_panel.current_directory.length() >= current_path.length()) {
                std::string substr = _other_panel.current_directory.substr(0, current_path.length());
                if (substr == current_path) {
                    //_other_panel.current_directory = current_directory;
                    std::filesystem::path new_path(_other_panel.current_directory);
                    while(!exists(new_path)) {
                        new_path = new_path.parent_path();
                    }
                    _other_panel.current_directory = new_path.string();
                }
            }
        }
        read_current_dir();
        _other_panel.read_current_dir();
        if (current_ind >= content.size()) {
            current_ind = content.size() - 1;
        }
        if (_other_panel.current_ind >= _other_panel.content.size()) {
            _other_panel.current_ind = _other_panel.content.size() - 1;
        }
    } else {
        type = create_remove_panel(HEADER_DELETE, "Delete: " + content[current_ind].name_content,
                                   HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL);
        if (type == REMOVE_TYPE::REMOVE_ALL || type == REMOVE_TYPE::REMOVE_THIS) {
            try {
                bool is_dir = false;
                if (is_directory(p)) {
                    is_dir = true;
                }
                std::filesystem::remove(p);
                if (is_dir) {
                    _other_panel.current_directory = current_directory;
                }
                read_current_dir();
                _other_panel.read_current_dir();
                if (current_ind >= content.size()) {
                    current_ind = content.size() - 1;
                }
                if (_other_panel.current_ind >= _other_panel.content.size()) {
                    _other_panel.current_ind = _other_panel.content.size() - 1;
                }
            } catch (std::filesystem::filesystem_error &e) {
                int error_ind = e.code().value();
                if (error_ind == 13) {
                    generate_permission_error(e);
                }
            }
        }
    }
}

void file_panel::sequential_removing(std::filesystem::path &_p, file_panel &_other_panel, bool _all) {
    try {
        if (is_symlink(_p)) {
            std::filesystem::remove(_p);
            read_current_dir();
            if (_other_panel.current_directory == current_directory) {
                _other_panel.read_current_dir();
            }
            return;
        }
    } catch (std::filesystem::filesystem_error &e) {
        int error_ind = e.code().value();
        if (error_ind == 13) {
            generate_permission_error(e);
        }
    }
    REMOVE_TYPE type;
    size_t index = _p.string().rfind(content[current_ind].name_content);
    std::stack<std::filesystem::path> dir_stack;
    bool flag_delete_other = _all;
    dir_stack.push(_p);
    while (!dir_stack.empty()) {
        std::filesystem::path current_path = dir_stack.top();
        dir_stack.pop();
        for (const auto &entry: std::filesystem::directory_iterator(current_path,
                                                                    std::filesystem::directory_options::skip_permission_denied)) {
            //bool delete_empty_subdir = false;
            if (!flag_delete_other) {
                std::string message = "Delete: " + entry.path().string().substr(index);
                type = create_remove_panel(HEADER_DELETE, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                           WEIGHT_FUNCTIONAL_PANEL - 1 > message.length() ? WEIGHT_FUNCTIONAL_PANEL :
                                           message.length() + 2);
            }
            try {
                if (type == REMOVE_TYPE::REMOVE_ALL) {
                    flag_delete_other = true;
                }
                if (type == REMOVE_TYPE::REMOVE_THIS || flag_delete_other) {
                    if (entry.is_directory()) {
                        std::filesystem::remove_all(entry.path());
                    } else std::filesystem::remove(entry.path());
                }

            } catch (std::filesystem::filesystem_error &e) {
                int error_ind = e.code().value();
                if (error_ind == 13) {
                    generate_permission_error(e);
                }
            }
            display_content();
            _other_panel.display_content();
            if (type == REMOVE_TYPE::STOP_REMOVE) {
                return;
            }
            if (std::filesystem::is_directory(entry.path())) {
                dir_stack.push(entry.path());
            }
        }
    }
    if (std::filesystem::is_empty(_p)) {
        if (flag_delete_other) {
            std::filesystem::remove(_p);
        } else {
            display_content();
            _other_panel.display_content();
            std::string message = "Delete: " + _p.string().substr(index);
            type = create_remove_panel(HEADER_DELETE, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                       WEIGHT_FUNCTIONAL_PANEL - 1 > message.length()
                                       ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
            if (type == REMOVE_TYPE::REMOVE_ALL || type == REMOVE_TYPE::REMOVE_THIS) {
                std::filesystem::remove(_p);
            }
        }
    }
}

void file_panel::overwrite_content_move(file_panel &_other_panel, std::filesystem::path &_from, std::filesystem::path &_to) {
    bool overwrite_other = false;
    REMOVE_TYPE type;
    std::string message = "Overwrite: " + _to.string() + "/" + content[current_ind].name_content;
    type = create_remove_panel(HEADER_MOVE, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                               WEIGHT_FUNCTIONAL_PANEL > message.length()
                               ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
    if (type == REMOVE_TYPE::REMOVE_ALL) {
        overwrite_other = true;
    } else if (type == REMOVE_TYPE::STOP_REMOVE || type == REMOVE_TYPE::SKIP) {
        return;
    }
    size_t index = _from.string().rfind(content[current_ind].name_content);
    std::stack<std::filesystem::path> dir_stack;
    dir_stack.push(_from);
    while(!dir_stack.empty()) {
        std::filesystem::path current_path = dir_stack.top();
        bool flag_is_empty_after_move = false;
        dir_stack.pop();
        for (const auto& entry : std::filesystem::directory_iterator(current_path, std::filesystem::directory_options::skip_permission_denied)) {
            bool flag_skip = false;
            std::string path_string = entry.path().string();
            std::filesystem::path copy_part(path_string.substr(index));
            //std::filesystem::path copy_part(path_string.substr(path_string.rfind(content[current_ind].name_content)));
            std::filesystem::path full_copy_to(_to / copy_part);
            if (!exists(full_copy_to)) {
                flag_skip = true;
                try {
                    std::filesystem::rename(entry.path(), full_copy_to);
                    flag_is_empty_after_move = true;
                } catch (std::filesystem::filesystem_error &e) {
                    display_content();
                    _other_panel.display_content();
                    int error_ind = e.code().value();
                    if (error_ind == 13) {
                        generate_permission_error(e);
                    } else if (error_ind == 21 || error_ind == 22) {
                        generate_incompatible_error(e);
                    }
                }
            } else {
                if (!overwrite_other) {
                    display_content();
                    _other_panel.display_content();
                    message = "Overwrite: " + path_string;
                    type = create_remove_panel(HEADER_MOVE, message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                               WEIGHT_FUNCTIONAL_PANEL > message.length()
                                               ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
                }
                if (type == REMOVE_TYPE::REMOVE_ALL) {
                    display_content();
                    _other_panel.display_content();
                    overwrite_other = true;
                }
                if (type == REMOVE_TYPE::STOP_REMOVE) {
                    return;
                } else if (type == REMOVE_TYPE::REMOVE_THIS || overwrite_other) {
                    try {
                        bool is_correct_types = true;
                        if ((entry.is_directory() && !entry.is_symlink())) {
                            if (is_directory(full_copy_to) && !is_symlink(full_copy_to)) {
                                is_correct_types = true;
                            } else {
                                is_correct_types = false;
                                flag_skip = true;
                            }
                        } else if ((entry.is_symlink() && entry.is_directory()) ||
                                   (entry.is_symlink() && !entry.is_directory())) {
                            if (!is_symlink(full_copy_to)) {
                                is_correct_types = false;
                            }
                        } else if (!entry.is_symlink()) {
                            if (is_symlink(full_copy_to)) {
                                is_correct_types = false;
                            }
                        }
                        if ((is_correct_types) &&
                            (entry.is_socket() || entry.is_regular_file() || entry.is_regular_file()
                             || entry.is_block_file() || entry.is_fifo() || entry.is_character_file() ||
                             entry.is_symlink())) {
                            std::filesystem::remove(full_copy_to);
                            std::filesystem::rename(entry.path(), full_copy_to);
                            flag_is_empty_after_move = true;
                        } else if (!is_correct_types) {
                            throw std::filesystem::filesystem_error("Another types", entry.path(), full_copy_to,
                                                                    std::error_code(21, std::iostream_category()));
                        }
                    } catch (std::filesystem::filesystem_error &e) {
                        display_content();
                        _other_panel.display_content();
                        int error_ind = e.code().value();
                        if (error_ind == 13) {
                            generate_permission_error(e);
                        } else if (error_ind == 21 || error_ind == 22) {
                            generate_incompatible_error(e);
                        }
                    }
                }
            }
            if (!flag_skip && entry.is_directory() /*&& !overwrite_other*/) {
                dir_stack.push(entry.path());
            }
        }
        if (flag_is_empty_after_move && std::filesystem::is_empty(current_path)) {
            std::filesystem::remove(current_path);
        }
    }
}

void generate_permission_error(std::filesystem::filesystem_error &e) {
    std::string message = e.path1().string();
    if (message.empty()) {
        message = "Recursive error. Check files for access reading";
    }
    int len = static_cast<int>(message.length());
    create_error_panel(" Permission error ", message,
                       HEIGHT_FUNCTIONAL_PANEL - 2,
                       WEIGHT_FUNCTIONAL_PANEL > len ? WEIGHT_FUNCTIONAL_PANEL : len + 2);
}

void generate_incompatible_error(std::filesystem::filesystem_error &e) {
    std::string message = e.path1().string();
    int len = static_cast<int>(message.length());
    create_error_panel(" Incompatible types ", message,
                       HEIGHT_FUNCTIONAL_PANEL - 2,
                       WEIGHT_FUNCTIONAL_PANEL > len ? WEIGHT_FUNCTIONAL_PANEL : len + 2);
}

info::info(std::string_view _name_content,
           std::string_view _last_redact_content,
           ssize_t _current_ind,
           CONTENT_TYPE _content_type,
           COLOR_INDEX _color_index) {
    this->content_type = _content_type;
    this->name_content = _name_content;
    this->last_redact_content = _last_redact_content;
    this->size_content = _current_ind;
    this->color_index = _color_index;
}

WINDOW *create_functional_panel(const std::string &_header, int height, int weight) {
    WINDOW *win = newwin(height, weight,
                         (LINES - height) / 2,
                         (COLS - weight) / 2);
    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(4));
    wattron(win, COLOR_PAIR(5));
    wattron(win, A_BOLD);
    mvwprintw(win, 0, (weight - static_cast<int>(_header.length())) / 2, "%s", _header.c_str());
    wattroff(win, A_BOLD);
    wattroff(win, COLOR_PAIR(5));

    wbkgd(win, COLOR_PAIR(4));
    wrefresh(win);

    curs_set(1);
    return win;
}

bool symlink_hardlink_func_panel(const std::string &_header,
                                 std::string &_namelink,
                                 std::string &_pointer, int _height, int _weight) {
    WINDOW *win = create_functional_panel(_header, _height, _weight);
    FIELD *fields[SIZE_FIELD_BUFFER_1];
    fields[0] = new_field(1, LEN_LINE_FIRST, 1, 11, 0, 0);
    fields[1] = new_field(1, LEN_LINE_FIRST, 3, 11, 0, 0);
    fields[2] = new_field(1, static_cast<int>(strlen(OK_BUTTON)), 5, 17, 0, 0);
    fields[3] = new_field(1, static_cast<int>(strlen(NO_BUTTON)), 5, 35, 0, 0);
    fields[4] = nullptr;
    set_field_back(fields[0], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[1], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[2], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[3], COLOR_PAIR(7) | A_BOLD);
    set_field_buffer(fields[2], 0, OK_BUTTON);
    set_field_buffer(fields[3], 0, NO_BUTTON);
    FORM *my_form = new_form(fields);
    set_form_win(my_form, win);

    WINDOW *subwin = derwin(win, _height - 4, _weight - 2, 2, 1);

    set_form_sub(my_form, subwin);
    post_form(my_form);

    wattron(subwin, COLOR_PAIR(5));
    wattron(subwin, A_BOLD);
    mvwprintw(subwin, 0, 11, "%s", DESCRIPTION_LINK);
    mvwprintw(subwin, 2, 11, "%s", DESCRIPTION_LINK_POINTER);
    wattroff(subwin, A_BOLD);
    wattroff(subwin, COLOR_PAIR(5));

    wrefresh(subwin);
    wrefresh(win);
    pos_form_cursor(my_form);
    wrefresh(win);

    bool entry_flag = navigation_symlink_create_panel(win, my_form, fields, _namelink, _pointer);

    for (size_t i = 0; i < SIZE_FIELD_BUFFER_1 - 1; i++) {
        free_field(fields[i]);
    }

    free_form(my_form);
    delwin(subwin);
    delwin(win);
    curs_set(0);

    return entry_flag;
}

void create_error_panel(const std::string &_header, const std::string &_message, int _height, int _weight) {
    WINDOW *win = newwin(_height, _weight,
                         (LINES - _height) / 2,
                         (COLS - _weight) / 2);
    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(9));
    wattron(win, COLOR_PAIR(9));
    wattron(win, A_BOLD);
    mvwprintw(win, 0, (_weight - static_cast<int>(_header.length())) / 2, "%s", _header.c_str());

    wrefresh(win);

    mvwprintw(win, _height / 2 - 1,
              (_weight - static_cast<int>(_message.length())) / 2,
              "%s", _message.c_str());
    mvwprintw(win, _height - 1,
              (_weight - static_cast<int>(strlen(PRESS_ANY_BUTTON))) / 2,
              "%s", PRESS_ANY_BUTTON);
    wrefresh(win);
    getch();
    wattroff(win, A_BOLD);
    wattroff(win, COLOR_PAIR(9));
    delwin(win);
}

bool navigation_symlink_create_panel(WINDOW *_win, FORM *_form, FIELD **_fields, std::string &_namelink,
                                     std::string &_pointer) {
    int ch;
    std::string *current_buffer = &_namelink;
    size_t index_first_field = 0;
    size_t index_second_field = _pointer.length();
    size_t index_field = 0;
    int offset_first_field = 0;
    int offset_second_field = 0;
    int *current_offset_field = &offset_first_field;
    size_t *current_index = &index_first_field;

    if (_pointer.length() > LEN_LINE_FIRST - 1) {
        offset_second_field = static_cast<int>(index_second_field) - LEN_LINE_FIRST + 1;
    }
    set_current_field(_form, _fields[1]);
    display_buffer_on_form(_form, _pointer, &index_second_field, offset_second_field, LEN_LINE_FIRST);
    set_current_field(_form, _fields[0]);
    wrefresh(_win);

    while (true) {
        switch (ch = getch()) {
            case '\t' : {
                form_driver(_form, REQ_NEXT_FIELD);
                index_field = field_index(current_field(_form));
                if (index_field == 0) {
                    current_index = &index_first_field;
                    current_buffer = &_namelink;
                    current_offset_field = &offset_first_field;
                    set_field_back(_fields[3], COLOR_PAIR(7) | A_BOLD);
                    curs_set(1);
                } else if (index_field == 1) {
                    current_index = &index_second_field;
                    current_buffer = &_pointer;
                    current_offset_field = &offset_second_field;
                    curs_set(1);
                } else if (index_field == 2) {
                    set_field_back(_fields[index_field], COLOR_PAIR(8) | A_BOLD);
                    curs_set(0);
                } else {
                    set_field_back(_fields[index_field], COLOR_PAIR(8) | A_BOLD);
                    set_field_back(_fields[2], COLOR_PAIR(7) | A_BOLD);
                    curs_set(0);
                }
                break;
            }
            case KEY_RESIZE : {
                return false;
            }
            case '\n' : {
                if (index_field == 2) {
                    return true;
                } else {
                    return false;
                }
            }
            case KEY_LEFT : {
                if (is_input_field_link(index_field)) {
                    move_cursor_left_from_input_field(current_index, current_offset_field);
                }
                break;
            }
            case KEY_RIGHT : {
                if (is_input_field_link(index_field)) {
                    move_cursor_right_from_input_field(current_buffer->length(),
                                                       current_index, current_offset_field);
                }
                break;
            }
            case KEY_BACKSPACE : {
                if (is_input_field_link(index_field)) {
                    delete_char_from_input_field(*current_buffer, current_index,
                                                 current_offset_field);
                }
                break;
            }
            default: {
                if (is_input_field_link(index_field)) {
                    insert_char_from_input_field(*current_buffer, current_index,
                                                 current_offset_field, ch, LEN_LINE_FIRST);
                }
                break;
            }
        }
        if (is_input_field_link(index_field)) {
            display_buffer_on_form(_form, *current_buffer, current_index,
                                   *current_offset_field, LEN_LINE_FIRST);
        }
        wrefresh(_win);
    }
}

void display_buffer_on_form(FORM *_form, const std::string &_buffer,
                            const size_t *_ind, int _offset, size_t LEN_LINE) {
    if (_buffer.length() < LEN_LINE - 1 + _offset) {
        set_field_buffer(current_field(_form), 0, _buffer.c_str());
    } else {
        set_field_buffer(current_field(_form), 0, _buffer.substr(_offset, LEN_LINE - 1).c_str());
    }
    for (size_t i = 0; i < *_ind - _offset; i++) {
        form_driver(_form, REQ_RIGHT_CHAR);
    }
}

bool is_input_field_link(size_t _index) {
    if (_index == 0 || _index == 1) {
        return true;
    }
    return false;
}

void delete_char_from_input_field(std::string &_current_buffer,
                                  size_t *_current_index,
                                  int *_offset_field) {
    if (!_current_buffer.empty() && *_current_index != 0) {
        _current_buffer.erase((*_current_index) - 1, 1);
        (*_current_index)--;
    }
    if (*_offset_field != 0) {
        (*_offset_field)--;
    }
}

void insert_char_from_input_field(std::string &_current_buffer,
                                  size_t *_current_index,
                                  int *_current_offset_field,
                                  int ch, size_t LEN_LINE) {
    _current_buffer.insert(_current_buffer.begin() + static_cast<long>(*_current_index), static_cast<char>(ch));
    (*_current_index)++;
    if (*_current_index > LEN_LINE - 1 + *_current_offset_field) {
        (*_current_offset_field)++;
    }
}

void init_colors() {
    init_pair(1, COLOR_CYAN, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(4, COLOR_WHITE, COLOR_BLUE);
    init_pair(5, COLOR_RED, COLOR_BLUE);
    init_pair(6, COLOR_WHITE, COLOR_CYAN);
    init_pair(7, COLOR_WHITE, COLOR_BLUE);
    init_pair(8, COLOR_RED, COLOR_BLUE);
    init_pair(9, COLOR_WHITE, COLOR_RED);
    init_pair(10, COLOR_YELLOW, COLOR_BLUE);
    init_pair(11, COLOR_YELLOW, COLOR_BLUE);
    init_pair(12, COLOR_YELLOW, COLOR_BLUE);

    init_pair(WHITE_COLOR, COLOR_WHITE, COLOR_BLACK);
    init_pair(GREEN_COLOR, COLOR_GREEN, COLOR_BLACK);
    init_pair(YELLOW_COLOR, COLOR_YELLOW, COLOR_BLACK);
    init_pair(BLUE_COLOR, COLOR_BLUE, COLOR_BLACK);
    init_pair(MAGENTA_COLOR, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(RED_COLOR, COLOR_RED, COLOR_BLACK);
}

void move_cursor_left_from_input_field(size_t *_current_index, int *_current_offset_field) {
    if (*_current_offset_field == *_current_index && *_current_index != 0) {
        (*_current_offset_field)--;
    }
    if (*_current_index != 0) {
        (*_current_index)--;
    }
}

void move_cursor_right_from_input_field(size_t _len, size_t *_current_index, int *_current_offset_field) {
    if (*_current_index < _len) {
        (*_current_index)++;
    }
    if (*_current_index > LEN_LINE_FIRST - 1 + *_current_offset_field) {
        (*_current_offset_field)++;
    }
}

bool create_redact_other_func_panel(const std::string &_header,
                                    const std::string &_description,
                                    std::string &_result, int _height, int _weight) {
    WINDOW *win = create_functional_panel(_header, _height, _weight);
    FIELD *fields[SIZE_FIELD_BUFFER_2];
    fields[0] = new_field(1, LEN_LINE_FIRST, 2, 11, 0, 0);
    fields[1] = new_field(1, static_cast<int>(strlen(OK_BUTTON)), 4, 17, 0, 0);
    fields[2] = new_field(1, static_cast<int>(strlen(NO_BUTTON)), 4, 35, 0, 0);
    fields[3] = nullptr;
    set_field_back(fields[0], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[1], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[2], COLOR_PAIR(7) | A_BOLD);
    set_field_buffer(fields[1], 0, OK_BUTTON);
    set_field_buffer(fields[2], 0, NO_BUTTON);

    FORM *my_form = new_form(fields);
    set_form_win(my_form, win);
    WINDOW *subwin = derwin(win, _height - 4, _weight - 2, 2, 1);
    set_form_sub(my_form, subwin);
    post_form(my_form);

    wattron(subwin, COLOR_PAIR(5));
    wattron(subwin, A_BOLD);
    mvwprintw(subwin, 1, 11, "%s", _description.c_str());
    wattroff(subwin, A_BOLD);
    wattroff(subwin, COLOR_PAIR(5));

    wrefresh(subwin);
    wrefresh(win);
    pos_form_cursor(my_form);
    wrefresh(win);

    bool entry_flag = navigation_functional_create_redact_panel(win, my_form, fields, _result);

    for (size_t i = 0; i < SIZE_FIELD_BUFFER_2 - 1; i++) {
        free_field(fields[i]);
    }

    free_form(my_form);
    delwin(subwin);
    delwin(win);
    curs_set(0);
    return entry_flag;
}

REMOVE_TYPE create_remove_panel(const std::string &_header, const std::string &description, int _height, int _weight) {
    WINDOW *win = create_functional_panel(_header, _height, _weight);
    FIELD *fields[SIZE_FIELD_BUFFER_1];
    curs_set(0);
    //35
    int center = (_weight - 35) / 2;
    fields[0] = new_field(1, static_cast<int>(strlen(YES_BUTTON)), _height - 5, center, 0, 0);
    fields[1] = new_field(1, static_cast<int>(strlen(ALL_BUTTON)), _height - 5, center + 9, 0, 0);
    fields[2] = new_field(1, static_cast<int>(strlen(NO_BUTTON)), _height - 5, center + 9 + 9, 0, 0);
    fields[3] = new_field(1, static_cast<int>(strlen(STOP_BUTTON)), _height - 5, center + 9 + 9 + 8, 0, 0);
    fields[4] = nullptr;

    set_field_back(fields[0], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[1], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[2], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[3], COLOR_PAIR(7) | A_BOLD);
    set_field_buffer(fields[0], 0, YES_BUTTON);
    set_field_buffer(fields[1], 0, ALL_BUTTON);
    set_field_buffer(fields[2], 0, NO_BUTTON);
    set_field_buffer(fields[3], 0, STOP_BUTTON);

    FORM *my_form = new_form(fields);
    set_form_win(my_form, win);
    WINDOW *subwin = derwin(win, _height - 4, _weight - 2, 2, 1);
    set_form_sub(my_form, subwin);
    post_form(my_form);

    wattron(win, COLOR_PAIR(5));
    wattron(win, A_BOLD);
    mvwprintw(win, _height / 2 - 1, (_weight - static_cast<int>(description.length())) / 2, "%s",
              (description).c_str());
    wattroff(win, COLOR_PAIR(5));
    wattroff(win, COLOR_PAIR(5));

    wrefresh(subwin);
    wrefresh(win);
    pos_form_cursor(my_form);
    wrefresh(win);

    REMOVE_TYPE flag = navigation_remove(win, my_form, fields);

    for (size_t i = 0; i < SIZE_FIELD_BUFFER_1 - 1; i++) {
        free_field(fields[i]);
    }

    free_form(my_form);
    delwin(subwin);
    delwin(win);
    return flag;
}

REMOVE_TYPE navigation_remove(WINDOW *_win, FORM *_form, FIELD **_fields) {
    int current_ind = -1;
    while (true) {
        switch (getch()) {
            case '\t' : {
                current_ind = field_index(current_field(_form));
                if (current_ind == 0) {
                    set_field_back(_fields[SIZE_FIELD_BUFFER_1 - 2], A_BOLD | COLOR_PAIR(7));
                } else {
                    set_field_back(_fields[current_ind - 1], A_BOLD | COLOR_PAIR(7));
                }
                set_field_back(_fields[current_ind], A_BOLD | COLOR_PAIR(8));
                form_driver(_form, REQ_NEXT_FIELD);
                break;
            }
            case KEY_RESIZE : {
                return REMOVE_TYPE::STOP_REMOVE;
            }
            case '\n' : {
                if (current_ind == 0) {
                    return REMOVE_TYPE::REMOVE_THIS;
                } else if (current_ind == 1) {
                    return REMOVE_TYPE::REMOVE_ALL;
                } else if (current_ind == 2) {
                    return REMOVE_TYPE::SKIP;
                } else return REMOVE_TYPE::STOP_REMOVE;
            }
        }
        wrefresh(_win);
    }
}


bool change_permissions_panel(const std::string &_header,
                              const std::string &_description,
                              int _height, int _weight,
                              char_permissions &_perms) {

    WINDOW *win = create_functional_panel(_header, _height, _weight);
    curs_set(0);
    FIELD *fields[SIZE_FIELD_BUFFER_3];
    fields[0] = new_field(1, 3, 3, 9, 0, 0);
    fields[1] = new_field(1, 3, 3, 16, 0, 0);
    fields[2] = new_field(1, 3, 3, 23, 0, 0);
    fields[3] = new_field(1, 1, 3, 47, 0, 0);
    fields[4] = new_field(1, static_cast<int>(strlen(OK_BUTTON)), _height - 5, 17, 0, 0);
    fields[5] = new_field(1, static_cast<int>(strlen(NO_BUTTON)), _height - 5, 35, 0, 0);
    fields[6] = nullptr;

    std::string old_owner_perm = _perms.owner_perm;
    std::string old_group_perm = _perms.group_perm;
    std::string old_other_perm = _perms.other_perm;
    char old_recursive = _perms.recursive;

    FORM *my_form = new_form(fields);
    set_form_win(my_form, win);
    WINDOW *subwin = derwin(win, _height - 4, _weight - 2, 2, 1);
    set_form_sub(my_form, subwin);
    post_form(my_form);

    wattron(subwin, A_BOLD | COLOR_PAIR(8));
    mvwprintw(subwin, 0, 2, "File: %s", _description.c_str());
    wattroff(subwin, A_BOLD | COLOR_PAIR(8));
    wattron(subwin, A_BOLD | COLOR_PAIR(7));
    mvwprintw(subwin, 2, 8, "%s", "owner");
    mvwprintw(subwin, 2, 15, "%s", "group");
    mvwprintw(subwin, 2, 22, "%s", "other");
    mvwprintw(subwin, 3, 2, "%s", "new:");
    mvwprintw(subwin, 4, 2, "%s", "old:");
    mvwprintw(subwin, 3, 8, "%s", "[   ]");
    mvwprintw(subwin, 3, 15, "%s", "[   ]");
    mvwprintw(subwin, 3, 22, "%s", "[   ]");
    mvwprintw(subwin, 4, 8, "[%s]", old_owner_perm.c_str());
    mvwprintw(subwin, 4, 15, "[%s]", old_group_perm.c_str());
    mvwprintw(subwin, 4, 22, "[%s]", old_other_perm.c_str());
    mvwprintw(subwin, 2, 43, "%s", "recursive");
    mvwprintw(subwin, 3, 46, "%s", "[ ]");
    mvwprintw(subwin, 4, 46, "[%c]", old_recursive);
    wattroff(subwin, A_BOLD | COLOR_PAIR(7));

    set_field_back(fields[0], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[1], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[2], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[3], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[4], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[5], COLOR_PAIR(7) | A_BOLD);
    set_field_buffer(fields[0], 0, old_owner_perm.c_str());
    set_field_buffer(fields[1], 0, old_group_perm.c_str());
    set_field_buffer(fields[2], 0, old_other_perm.c_str());
    set_field_buffer(fields[3], 0, "-");
    set_field_buffer(fields[4], 0, OK_BUTTON);
    set_field_buffer(fields[5], 0, NO_BUTTON);

    wrefresh(subwin);
    wrefresh(win);
    pos_form_cursor(my_form);
    wrefresh(win);

    bool entry_flag = navigation_edit_permissions(win, my_form, fields, _perms);

    for (size_t i = 0; i < SIZE_FIELD_BUFFER_3 - 1; i++) {
        free_field(fields[i]);
    }

    free_form(my_form);
    delwin(subwin);
    delwin(win);
    return entry_flag;
}


void find_utility(file_panel& first, file_panel& second, const std::string& _current_dir) {
    std::string _query("*");
    char_permissions perms_str;
    perms_str.group_perm = "---";
    perms_str.other_perm = "---";
    perms_str.owner_perm = "---";
    char take_dir = 'X';
    char take_lnk = 'X';
    char take_reg = 'X';


    std::string min_size;
    std::string max_size;
    bool flag_entry = create_find_panel(_current_dir, _query, min_size,
                                        max_size, take_reg, take_dir, take_lnk, perms_str);
    if (flag_entry) {
        bool flag_perms;
        bool flag_dir, flag_reg, flag_lnk;
        take_dir == 'X' ? flag_dir = true : flag_dir = false;
        take_reg == 'X' ? flag_reg = true : flag_reg = false;
        take_lnk == 'X' ? flag_lnk = true : flag_lnk = false;

        std::filesystem::perms find_perms = std::filesystem::perms::none;
        fill_permissions(perms_str, find_perms);
        find_perms == std::filesystem::perms::none ? flag_perms = false : flag_perms = true;
        std::vector<std::string> results;
        bool is_good_query = find_collect_results(_current_dir, _query, results,
                                                  flag_reg, flag_dir, flag_lnk, flag_perms,
                                                  find_perms);
        if (is_good_query) {
            first.display_content();
            second.display_content();
            std::string return_result;
            create_find_content_panel(results, return_result);
            std::filesystem::path p(return_result);
            if (std::filesystem::exists(p)) {
                file_panel* current_panel;
                if (first.is_active_panel()) {
                    current_panel = &first;
                } else {
                    current_panel = &second;
                }
                bool is_dir = true;
                std::string buffer;
                if (!is_directory(p)) {
                    buffer = p.filename().string();
                    p = p.parent_path();
                    is_dir = false;
                }
                if ((status(p).permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none
                    || (status(p).permissions() & std::filesystem::perms::owner_exec) == std::filesystem::perms::none) {
                    first.display_content();
                    second.display_content();
                    std::string message = "Cannot open directory: '" + p.filename().string() + "'";
                    create_error_panel(" Permission error ", message, HEIGHT_FUNCTIONAL_PANEL,
                                       WEIGHT_FUNCTIONAL_PANEL > message.length()
                                       ? WEIGHT_FUNCTIONAL_PANEL : message.length());
                    return;
                }
                current_panel->set_current_directory(p.string());
                current_panel->read_current_dir();
                if (is_dir) {
                    current_panel->set_current_ind(0);
                    current_panel->set_start_ind(0);
                } else {
                    const auto& vec = current_panel->get_content();
                    size_t ind = 0;
                    for (auto && it : vec) {
                        if (it.name_content == buffer) {
                            current_panel->set_current_ind(ind);
                            current_panel->set_start_ind(static_cast<int>(ind / (LINES - 4)) * (LINES - 4));
                            break;
                        }
                        ++ind;
                    }
                }
            }
        }
    }
}

void create_find_content_panel(std::vector<std::string>& _content, std::string& return_result) {
    size_t max_len = 0;
    for (const auto & it : _content) {
        if (it.length() > max_len) {
            max_len = it.length();
        }
    }
    int height = 20;
    //int weight = max_len > WEIGHT_HISTORY_PANEL - 2 ? static_cast<int>(max_len) + 2 : WEIGHT_HISTORY_PANEL;
    int weight = max_len + 2 > COLS - 6 ? COLS - 6 : static_cast<int>(max_len + 2);
    WINDOW* win = newwin(height, weight, (LINES - height) / 2, (COLS - weight) / 2);
    wbkgd(win, COLOR_PAIR(6));
    size_t current_ind = 0;
    size_t start = 0;
    find_show_content(win, height, weight, start, current_ind, _content);
    bool flag_continue = true;
    while(flag_continue) {
        switch(getch()) {
            case KEY_RESIZE : {
                flag_continue = false;
                break;
            }
            case KEY_DOWN : {
                find_pagination(KEY_DOWN, height, start, current_ind, _content);
                find_show_content(win, height, weight, start, current_ind, _content);
                break;
            }
            case KEY_UP : {
                find_pagination(KEY_UP, height, start, current_ind, _content);
                find_show_content(win, height, weight, start, current_ind, _content);
                break;
            }
            case '\n' : {
                flag_continue = false;
                return_result = _content[current_ind];
                break;
            }
            case 'q' : {
                flag_continue = false;
                break;
            }
        }
    }
    delwin(win);
}

void find_pagination(size_t _direction, size_t _height, size_t &_start, size_t &_current_ind,
                     const std::vector<std::string> &_content) {
    if (_direction == KEY_UP) {
        if (_current_ind == 0) {
            _start = 0;
        } else if (_current_ind != _start) {
            _current_ind--;
        } else {
            _start -= _height - 2;
            _current_ind--;
        }
    } else if (_direction == KEY_DOWN) {
        if (_current_ind + 1 < _content.size()) {
            if (_current_ind + 1 >= _height - 2 + _start) {
                _start += _height - 2;
            }
            _current_ind++;
        }
    }
}

void find_show_content(WINDOW *_win, size_t _height, size_t _weight, size_t _start, size_t _current_ind, std::vector<std::string>& _content) {
    werase(_win);
    refresh();
    box(_win, 0, 0);
    wattron(_win, A_BOLD);
    mvwprintw(_win, 0, static_cast<int>(_weight - (strlen(" Find content "))) / 2, "%s", " Find content ");
    mvwprintw(_win, static_cast<int>(_height - 1), static_cast<int>((_weight - strlen("Accept[Enter]/Decline[q]")) / 2), "%s", "Accept[Enter]/Decline[q]");
    size_t offset = 1;
    for (size_t i = _start; i < _content.size() && i < (_height - 2) + _start; i++) {
        if (_current_ind == i) {
            wattron(_win, A_REVERSE);
        }
        mvwprintw(_win, static_cast<int>(offset), 1, "%*s", static_cast<int>(_weight - 2), " ");
        if (_weight - 2 < _content[i].length()) {
            std::string short_str;
            convert_str_to_window_size(short_str, _content[i], _weight - 2);
            mvwprintw(_win, static_cast<int>(offset), 1, "%s", short_str.c_str());
        } else {
            mvwprintw(_win, static_cast<int>(offset), 1, "%s", _content[i].c_str());
        }
        wattroff(_win, A_REVERSE);
        offset++;
    }
    refresh_sub_panel(_win);
    wattroff(_win, A_BOLD);
}

bool find_collect_results(const std::string& _current_dir, std::string &_query, std::vector<std::string>& _results,
                          bool flag_reg, bool flag_dir, bool flag_lnk, bool flag_perms, std::filesystem::perms& find_perms) {
    if (_query[0] == '*') {
        _query.erase(0, 1);
        for (const auto &entry: std::filesystem::recursive_directory_iterator(_current_dir,
                                                                              std::filesystem::directory_options::skip_permission_denied)) {
            if (entry.path().extension() == _query) {
                if ((entry.is_directory() && flag_dir) || (entry.is_symlink() && flag_lnk)
                || (entry.is_regular_file() && flag_reg)) {
                    if (flag_perms) {
                        if (entry.status().permissions() == find_perms) {
                            _results.push_back(entry.path());
                        }
                    }  else {
                        _results.push_back(entry.path());
                    }
                }
            }
        }
    } else {
        for (const auto & entry : std::filesystem::recursive_directory_iterator(_current_dir,
                                                                                std::filesystem::directory_options::skip_permission_denied)) {
            if (entry.path().filename() == _query) {
                if ((entry.is_directory() && flag_dir) || (entry.is_symlink() && flag_lnk)
                    || (entry.is_regular_file() && flag_reg)) {
                    if (flag_perms) {
                        if (entry.status().permissions() == find_perms) {
                            _results.push_back(entry.path());
                        }
                    } else {
                        _results.push_back(entry.path());
                    }
                }
            }
        }
    }



    if (_results.empty()) {
        return false;
    }
    return true;
}

bool create_find_panel(const std::string &_current_dir, std::string& _query,
                       std::string& min_size, std::string& max_size,
                       char& take_reg, char& take_dir, char& take_lnk,
                       char_permissions& perms_str) {
    int height = HEIGHT_FUNCTIONAL_PANEL + 9;
    int weight = WEIGHT_FUNCTIONAL_PANEL;
    WINDOW* win = create_functional_panel(" Find util ", height, weight);
    FIELD* fields[12];
    int left_offset = (weight - LEN_LINE_FIRST) / 2 - 1;
    fields[0] = new_field(1, LEN_LINE_FIRST, 2, left_offset, 0, 0);

    fields[1] = new_field(1, 3, 5, left_offset + 1 + 8, 0, 0);      //permissions
    fields[2] = new_field(1, 3, 5, left_offset + 8 + 8, 0, 0);
    fields[3] = new_field(1, 3, 5, left_offset + 15 + 8, 0, 0);

    fields[4] = new_field(1, 1, 8, left_offset + 1 + 8, 0, 0);          //types
    fields[5] = new_field(1, 1, 8, left_offset + 6 + 8, 0, 0);
    fields[6] = new_field(1, 1, 8, left_offset + 11 + 8, 0, 0);

    fields[7] = new_field(1, 7, 11, left_offset + 8, 0, 0);         //size
    fields[8] = new_field(1, 7, 11, left_offset + 19, 0, 0);

    fields[9] = new_field(1, 6, height - 5, 17, 0, 0);
    fields[10] = new_field(1, 6, height - 5, 35, 0, 0);

    fields[11] = nullptr;

    FORM* my_form = new_form(fields);
    set_form_win(my_form, win);
    WINDOW *subwin = derwin(win, height - 4, weight - 2, 2, 1);
    set_form_sub(my_form, subwin);
    post_form(my_form);
    wattron(subwin, A_BOLD | COLOR_PAIR(8));
    mvwprintw(subwin, 1, left_offset, "%s", "Name/extension:");



    mvwprintw(subwin, 5, left_offset, "%s", "Perms:");
    mvwprintw(subwin, 8, left_offset, "%s", "Types:");
    mvwprintw(subwin, 11, left_offset, "%s", "Size:");

    wattroff(subwin, A_BOLD | COLOR_PAIR(8));

    wattron(subwin, COLOR_PAIR(7) | A_BOLD);

    mvwprintw(subwin, 4, left_offset + 8, "%s", "owner");
    mvwprintw(subwin, 4, left_offset + 7 + 8, "%s", "group");
    mvwprintw(subwin, 4, left_offset + 14 + 8, "%s", "other");

    mvwprintw(subwin, 7, left_offset + 8, "%s", "dir");
    mvwprintw(subwin, 7, left_offset + 5 + 8, "%s", "reg");
    mvwprintw(subwin, 7, left_offset + 10 + 8, "%s", "lnk");

    mvwprintw(subwin, 5, left_offset + 8, "%s", "[   ]");
    mvwprintw(subwin, 5, left_offset + 7 + 8, "%s", "[   ]");
    mvwprintw(subwin, 5, left_offset + 14 + 8, "%s", "[   ]");

    mvwprintw(subwin, 8, left_offset + 8, "%s", "[ ]");
    mvwprintw(subwin, 8, left_offset + 5 + 8, "%s", "[ ]");
    mvwprintw(subwin, 8, left_offset + 9 + 8, " %s", "[ ]");

    mvwprintw(subwin, 11, left_offset + 19 - 3, "%s", "to");

    wattroff(subwin, COLOR_PAIR(7) | A_BOLD);

    set_field_back(fields[0], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[1], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[2], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[3], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[4], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[5], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[6], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[7], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[8], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[9], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[10], COLOR_PAIR(7) | A_BOLD);

    set_field_buffer(fields[1], 0, perms_str.owner_perm.c_str());
    set_field_buffer(fields[2], 0, perms_str.group_perm.c_str());
    set_field_buffer(fields[3], 0, perms_str.other_perm.c_str());
    set_field_buffer(fields[4], 0, "X");
    set_field_buffer(fields[5], 0, "X");
    set_field_buffer(fields[6], 0, "X");
    set_field_buffer(fields[9], 0, OK_BUTTON);
    set_field_buffer(fields[10], 0, NO_BUTTON);

    wrefresh(win);
    pos_form_cursor(my_form);
    wrefresh(win);

    bool entry_flag = navigation_find_utility(win, my_form, fields,
                                              _current_dir,
                                              min_size, max_size,
                                              take_reg, take_dir, take_lnk,
                                              perms_str, _query);
    for (size_t i = 0; i < 10; i++) {
        free_field(fields[i]);
    }

    free_form(my_form);
    delwin(subwin);
    delwin(win);
    return entry_flag;
}

bool navigation_find_utility(WINDOW *_win, FORM *_form, FIELD **_fields,
                             const std::string& _dir,
                             std::string& _min_size,
                             std::string& _max_size,
                             char& take_file, char& take_dir, char& take_lnk,
                             char_permissions& _str_perms,
                             std::string &_query) {
    int ch;

    size_t query_index = _query.length();
    size_t min_size_index = 0;
    size_t max_size_index = 0;

    int query_offset = 0;
    int min_size_offset = 0;
    int max_size_offset = 0;

    std::string* current_buffer = &_query;

    size_t* current_index = &query_index;
    int* current_offset = &query_offset;

    size_t index_field = 0;

    display_buffer_on_form(_form, _query, current_index, query_offset, LEN_LINE_FIRST);

    auto edit_func = [&](char ch, size_t _ind) {
        if (index_field == 1) {
            _str_perms.owner_perm[_ind] == ch ? _str_perms.owner_perm[_ind] = '-' : _str_perms.owner_perm[_ind] = ch;
            set_field_buffer(_fields[index_field], 0, _str_perms.owner_perm.c_str());
        } else if (index_field == 2) {
            _str_perms.group_perm[_ind] == ch ? _str_perms.group_perm[_ind] = '-' : _str_perms.group_perm[_ind] = ch;
            set_field_buffer(_fields[index_field], 0, _str_perms.group_perm.c_str());
        } else if (index_field == 3) {
            _str_perms.other_perm[_ind] == ch ? _str_perms.other_perm[_ind] = '-' : _str_perms.other_perm[_ind] = ch;
            set_field_buffer(_fields[index_field], 0, _str_perms.other_perm.c_str());
        }
    };

    wrefresh(_win);
    while (true) {
        switch (ch = getch()) {
            case '\t': {
                form_driver(_form, REQ_NEXT_FIELD);
                index_field = field_index(current_field(_form));
                if (index_field == 0) {
                    set_field_back(_fields[10], COLOR_PAIR(7) | A_BOLD);
                    current_buffer = &_query;
                    current_index = &query_index;
                    current_offset = &query_offset;
                    curs_set(1);
                } else if (index_field == 1 || index_field == 9) {
                    set_field_back(_fields[index_field], COLOR_PAIR(8) | A_BOLD);
                    curs_set(0);
                } else if (index_field == 2 || index_field == 3
                || index_field == 4 || index_field == 5 || index_field == 6
                || index_field == 10) {
                    set_field_back(_fields[index_field], COLOR_PAIR(8) | A_BOLD);
                    set_field_back(_fields[index_field - 1], COLOR_PAIR(7) | A_BOLD);
                } else if (index_field == 7) {
                    set_field_back(_fields[index_field - 1], COLOR_PAIR(7) | A_BOLD);
                    curs_set(1);
                    current_buffer = &_min_size;
                    current_index = &min_size_index;
                    current_offset = &min_size_offset;
                } else if (index_field == 8){
                    current_buffer = &_max_size;
                    current_index = &max_size_index;
                    current_offset = &max_size_offset;
                }
                break;
            }
            case KEY_RESIZE : {
                return false;
            }
            case '\n' : {
                if (index_field == 4) {
                    take_dir == 'X' ? take_dir = '-' : take_dir = 'X';
                    char dir[1] = {take_dir};
                    set_field_buffer(_fields[index_field], 0, dir);
                } else if (index_field == 5) {
                    take_file == 'X' ? take_file = '-' : take_file = 'X';
                    char file[1] = {take_file};
                    set_field_buffer(_fields[index_field], 0, file);
                } else if (index_field == 6) {
                    take_lnk == 'X' ? take_lnk = '-' : take_lnk = 'X';
                    char lnk[1] = {take_lnk};
                    set_field_buffer(_fields[index_field], 0, lnk);
                } else if (index_field == 9) {
                    return true;
                } else if (index_field == 10){
                    return false;
                }
                break;
            }
            case KEY_LEFT : {
                if (is_input_field_dir_file(index_field)) {
                    move_cursor_left_from_input_field(current_index, current_offset);
                }
                break;
            }
            case KEY_RIGHT : {
                if (is_input_field_dir_file(index_field)) {
                    move_cursor_right_from_input_field(current_buffer->length(), current_index, current_offset);
                }
                break;
            }
            case KEY_BACKSPACE : {
                if (is_input_field_find(index_field)) {
                    delete_char_from_input_field(*current_buffer, current_index, current_offset);
                }
                break;
            }
            default : {
                if (index_field == 1 || index_field == 2 || index_field == 3) {
                    if (ch == 'r') {
                        edit_func('r', 0);
                    } else if (ch == 'w') {
                        edit_func('w', 1);
                    } else if (ch == 'x') {
                        edit_func('x', 2);
                    }
                } else if (is_input_field_find(index_field)) {
                    if (index_field == 7 || index_field == 8) {
                        insert_char_from_input_field(*current_buffer, current_index, current_offset, ch, 7);
                    } else {
                        insert_char_from_input_field(*current_buffer, current_index, current_offset, ch, LEN_LINE_FIRST);
                    }
                }
            }
        }
        if (is_input_field_find(index_field)) {
            if (index_field == 7 || index_field == 8) {
                display_buffer_on_form(_form, *current_buffer, current_index, *current_offset, 7);
            } else {
                display_buffer_on_form(_form, *current_buffer, current_index, *current_offset, LEN_LINE_FIRST);
            }
        }
        wrefresh(_win);
    }
}

bool navigation_edit_permissions(WINDOW *_win, FORM *_form, FIELD **_fields, char_permissions &_perms) {
    int current_ind = -1;
    wrefresh(_win);

    auto edit_func = [&](char ch, size_t _ind) {
        if (current_ind == 0) {
            _perms.owner_perm[_ind] == ch ? _perms.owner_perm[_ind] = '-' : _perms.owner_perm[_ind] = ch;
            set_field_buffer(_fields[current_ind], 0, _perms.owner_perm.c_str());
        } else if (current_ind == 1) {
            _perms.group_perm[_ind] == ch ? _perms.group_perm[_ind] = '-' : _perms.group_perm[_ind] = ch;
            set_field_buffer(_fields[current_ind], 0, _perms.group_perm.c_str());
        } else if (current_ind == 2) {
            _perms.other_perm[_ind] == ch ? _perms.other_perm[_ind] = '-' : _perms.other_perm[_ind] = ch;
            set_field_buffer(_fields[current_ind], 0, _perms.other_perm.c_str());
        }
    };

    while (true) {
        switch (getch()) {
            case '\t' : {
                current_ind = field_index(current_field(_form));
                if (current_ind == 0) {
                    set_field_back(_fields[SIZE_FIELD_BUFFER_3 - 2], A_BOLD | COLOR_PAIR(7));
                } else {
                    set_field_back(_fields[current_ind - 1], A_BOLD | COLOR_PAIR(7));
                }
                set_field_back(_fields[current_ind], A_BOLD | COLOR_PAIR(8));
                form_driver(_form, REQ_NEXT_FIELD);
                break;
            }
            case KEY_RESIZE : {
                return false;
            }
            case 'w' : {
                edit_func('w', 1);
                break;
            }
            case 'r' : {
                edit_func('r', 0);
                break;
            }
            case 'x' : {
                edit_func('x', 2);
                break;
            }
            case '\n' : {
                if (current_ind == 3) {
                    _perms.recursive == 'X' ? _perms.recursive = '-' : _perms.recursive = 'X';
                    char ch[1] = {_perms.recursive};
                    set_field_buffer(_fields[current_ind], 0, ch);
                } else if (current_ind == 4) {
                    return true;
                } else if (current_ind == 5) {
                    return false;
                }
                break;
            }
        }
        wrefresh(_win);
    }
}


bool navigation_functional_create_redact_panel(WINDOW *_win, FORM *_form,
                                               FIELD **_fields,
                                               std::string &_result) {
    int ch;
    size_t current_index = 0;
    int offset = 0;
    size_t index_field = 0;
    if (!_result.empty()) {
        if (_result.length() > LEN_LINE_FIRST - 1) {
            offset = static_cast<int>(_result.length()) - LEN_LINE_FIRST + 1;
        }
        current_index = _result.length();
        display_buffer_on_form(_form, _result, &current_index, offset, LEN_LINE_FIRST);
        wrefresh(_win);
    }
    while (true) {
        switch (ch = getch()) {
            case '\t': {
                form_driver(_form, REQ_NEXT_FIELD);
                index_field = field_index(current_field(_form));
                if (index_field == 0) {
                    set_field_back(_fields[2], COLOR_PAIR(7) | A_BOLD);
                    curs_set(1);
                } else if (index_field == 1) {
                    set_field_back(_fields[index_field], COLOR_PAIR(8) | A_BOLD);
                    curs_set(0);
                } else {
                    set_field_back(_fields[index_field], COLOR_PAIR(8) | A_BOLD);
                    set_field_back(_fields[1], COLOR_PAIR(7) | A_BOLD);
                    curs_set(0);
                }
                break;
            }
            case KEY_RESIZE : {
                return false;
            }
            case '\n' : {
                if (index_field == 1) {
                    return true;
                } else {
                    return false;
                }
            }
            case KEY_LEFT : {
                if (is_input_field_dir_file(index_field)) {
                    move_cursor_left_from_input_field(&current_index, &offset);
                }
                break;
            }
            case KEY_RIGHT : {
                if (is_input_field_dir_file(index_field)) {
                    move_cursor_right_from_input_field(_result.length(), &current_index, &offset);
                }
                break;
            }
            case KEY_BACKSPACE : {
                if (is_input_field_dir_file(index_field)) {
                    delete_char_from_input_field(_result, &current_index, &offset);
                }
                break;
            }
            default : {
                if (is_input_field_dir_file(index_field)) {
                    insert_char_from_input_field(_result, &current_index, &offset, ch, LEN_LINE_FIRST);
                }
            }
        }
        if (is_input_field_dir_file(index_field)) {
            display_buffer_on_form(_form, _result, &current_index, offset, LEN_LINE_FIRST);
        }
        wrefresh(_win);
    }
}

bool is_input_field_dir_file(size_t _index) {
    if (_index == 0) {
        return true;
    }
    return false;
}

void convert_to_output(std::string &_name, CONTENT_TYPE _type) {
    switch (_type) {
        case CONTENT_TYPE::IS_DIR : {
            _name.insert(0, 1, '/');
            break;
        }
        case CONTENT_TYPE::IS_LNK : {
            _name.insert(0, 1, '@');
            break;
        }
        case CONTENT_TYPE::IS_LNK_TO_DIR : {
            _name.insert(0, 1, '~');
            break;
        }
        case CONTENT_TYPE::IS_HANGING_LINK : {
            _name.insert(0, 1, '!');
        }
        default :
            break;
    }
}

void create_history_panel(std::string& return_result) {
    if (history_vec.history_path.empty()) {
        create_error_panel(" History error ", "History is empty.", HEIGHT_FUNCTIONAL_PANEL - 2, WEIGHT_FUNCTIONAL_PANEL);
        return;
    }
    //int weight = history_vec.max_len > WEIGHT_HISTORY_PANEL - 2 ? static_cast<int>(history_vec.max_len) + 2 : WEIGHT_HISTORY_PANEL;
    int weight = history_vec.max_len + 2 > COLS - 6 ? COLS - 6 : static_cast<int>(history_vec.max_len + 2);
    int height = 15;
    WINDOW* win = newwin(height, weight, (LINES - height) / 2, (COLS - weight) / 2);
    wbkgd(win, COLOR_PAIR(6));
    size_t start = 0;
    size_t current_ind = 0;
    history_show_content(win, height, weight, start, current_ind);
    bool flag_continue = true;
    while(flag_continue) {
        switch(getch()) {
            case KEY_RESIZE : {
                flag_continue = false;
                break;
            }
            case KEY_DOWN : {
                history_pagination(KEY_DOWN, height, start, current_ind);
                history_show_content(win, height, weight, start, current_ind);
                break;
            }
            case KEY_UP : {
                history_pagination(KEY_UP, height, start, current_ind);
                history_show_content(win, height, weight, start, current_ind);
                break;
            }
            case '\n' : {
                return_result = history_vec.history_path[current_ind];
                flag_continue = false;
                break;
            }
            case 'h' : {
                flag_continue = false;
                break;
            }
        }
    }
    delwin(win);
}

void history_show_content(WINDOW* _win, size_t _height, size_t _weight, size_t _start, size_t _current_ind) {
    werase(_win);
    refresh();
    box(_win, 0, 0);
    wattron(_win, A_BOLD);
    mvwprintw(_win, 0, static_cast<int>(_weight - (strlen(HISTORY_HEADER))) / 2, "%s", HISTORY_HEADER);
    mvwprintw(_win, static_cast<int>(_height - 1), static_cast<int>((_weight - strlen("Accept[Enter]/Decline[q]")) / 2), "%s", "Accept[Enter]/Decline[q]");
    size_t offset = 1;
    for (size_t i = _start; i < history_vec.history_path.size() && i < (_height - 2) + _start; i++) {
        if (_current_ind == i) {
            wattron(_win, A_REVERSE);
        }
        mvwprintw(_win, static_cast<int>(offset), 1, "%*s", static_cast<int>(_weight - 2), " ");
        if (_weight - 2 < history_vec.history_path[i].length()) {
            std::string short_str;
            convert_str_to_window_size(short_str, history_vec.history_path[i], _weight - 2);
            mvwprintw(_win, static_cast<int>(offset), 1, "%s", short_str.c_str());
        } else {
            mvwprintw(_win, static_cast<int>(offset), 1, "%s", history_vec.history_path[i].c_str());
        }
        wattroff(_win, A_REVERSE);
        offset++;
    }
    refresh_sub_panel(_win);
    wattroff(_win, A_BOLD);
}

void history_pagination(size_t _direction, size_t _height, size_t &_start, size_t &_current_ind) {
    if (_direction == KEY_UP) {
        if (_current_ind == 0) {
            _start = 0;
        } else if (_current_ind != _start) {
            _current_ind--;
        } else {
            _start -= _height - 2;
            _current_ind--;
        }
    } else if (_direction == KEY_DOWN) {
        if (_current_ind + 1 < history_vec.history_path.size()) {
            if (_current_ind + 1 >= _height - 2 + _start) {
                _start += _height - 2;
            }
            _current_ind++;
        }
    }
}

void refresh_sub_panel(WINDOW *_win) {
    wrefresh(_win);
    refresh();
}

void file_panel::analysis_selected_file() {
    clear();
    refresh();
    WINDOW* info_win = newwin(LINES, COLS, 0, 0);
    wattron(info_win, COLOR_PAIR(10));
    mvwprintw(info_win, LINES - 1, 0, "%*s", COLS, " ");
    mvwprintw(info_win, 0, 0, "%*s", COLS, " ");
    std::string file_message = "Information about: '" + content[current_ind].name_content + "'";
    std::string continue_message = "Press any button to continue";
    mvwprintw(info_win, 0, static_cast<int>((COLS - file_message.length()) / 2), "%s", file_message.c_str());
    mvwprintw(info_win, LINES - 1, static_cast<int>((COLS - continue_message.length()) / 2), "%s", continue_message.c_str());
    wattroff(info_win, COLOR_PAIR(10));

    wattron(info_win, COLOR_PAIR(GREEN_COLOR));
    int info_lines = 11;
    int start = (LINES - info_lines) / 2;
    std::string current_path_str = current_directory + "/" + content[current_ind].name_content;
    std::filesystem::path current_path(current_directory + "/" + content[current_ind].name_content);
    std::string type_and_name;
    if (is_directory(current_path) && !is_symlink(current_path)) {
        type_and_name += "Directory: ";
    } else if (is_regular_file(current_path)) {
        type_and_name += "Regular file: ";
    } else if (is_character_file(current_path)) {
        type_and_name += "Character file: ";
    } else if (is_fifo(current_path)) {
        type_and_name += "Fifo: ";
    } else if (is_block_file(current_path)) {
        type_and_name += "Block file: ";
    } else if (is_socket(current_path)) {
        type_and_name += "Socket: ";
    } else if (is_directory(current_path) && is_symlink(current_path)) {
        type_and_name += "Directory symlink: ";
    } else if (is_symlink(current_path)) {
        type_and_name += "Symlink: ";
    } else {
        type_and_name += "Unknown type: ";
    }

    struct stat sb {};
    lstat(current_path_str.c_str(), &sb);
    std::string path_str = "Path: " + current_path_str;
    std::string size_str = "Size: " + std::to_string(sb.st_size)
            + " bytes / " + "Blocks: " + std::to_string(sb.st_blocks)
            + " / " + "Block size: " + std::to_string(sb.st_blksize) + " bytes";

    std::filesystem::perms p = std::filesystem::status(current_path).permissions();
    std::string perms;
    auto parse_arg = [=](std::string &_buffer, char op, std::filesystem::perms perms) {
        std::filesystem::perms::none == (perms & p) ? _buffer.push_back('-') : _buffer.push_back(op);
    };

    parse_arg(perms, 'r', std::filesystem::perms::owner_read);
    parse_arg(perms, 'w', std::filesystem::perms::owner_write);
    parse_arg(perms, 'x', std::filesystem::perms::owner_exec);
    parse_arg(perms, 'r', std::filesystem::perms::group_read);
    parse_arg(perms, 'w', std::filesystem::perms::group_write);
    parse_arg(perms, 'x', std::filesystem::perms::group_exec);
    parse_arg(perms, 'r', std::filesystem::perms::others_read);
    parse_arg(perms, 'w', std::filesystem::perms::others_write);
    parse_arg(perms, 'x', std::filesystem::perms::others_exec);

    std::string mode_str = "Mode: " + perms + " (" +  + ")";


    std::string link_str = "Links: " + std::to_string(sb.st_nlink);
    getpwuid(sb.st_uid);
    auto uid_name = getpwuid(sb.st_uid);
    auto gid_name = getgrgid(sb.st_gid);
    std::string uid_name_str;
    std::string id_str = "User ID: " +  std::string(uid_name->pw_name) + " (" + std::to_string(sb.st_uid) + ") / "
            + "Group id: " + std::string(gid_name->gr_name) + " (" + std::to_string(sb.st_gid) + ")";
    char date[25];
    strftime(date, sizeof(date), "%D %H:%M:%S", std::gmtime(&sb.st_atim.tv_sec));
    std::string last_access = "Last access: " + std::string(date);
    strftime(date, sizeof(date), "%D %H:%M:%S", std::gmtime(&sb.st_ctim.tv_sec));
    std::string last_change = "Last change: " + std::string(date);
    strftime(date, sizeof(date), "%D %H:%M:%S", std::gmtime(&sb.st_mtim.tv_sec));
    std::string last_modification = "Last modification: " + std::string(date);
    std::string inode_str = "Inode: " + std::to_string(sb.st_ino);

    std::string command = "df " + current_path_str;

    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    char buffer[256];
    std::vector<std::string> a;
    while (!feof(pipe.get())) {
        if (fgets(buffer, 256, pipe.get()) != nullptr) {
            a.emplace_back(buffer);
        }
    }

    std::string filesystem_name = a[1].substr(0, a[1].find(' '));
    std::string file_system = "File system: " + filesystem_name + " (" + std::to_string(sb.st_dev) + ")";

    int len = static_cast<int>((COLS - path_str.length()) / 2);
    type_and_name += content[current_ind].name_content;
    mvwprintw(info_win, start++, len, "%s", type_and_name.c_str());
    mvwprintw(info_win, start++, len, "%s", path_str.c_str());
    mvwprintw(info_win, start++, len, "%s", size_str.c_str());
    mvwprintw(info_win, start++, len, "%s", mode_str.c_str());
    mvwprintw(info_win, start++, len, "%s", link_str.c_str());
    mvwprintw(info_win, start++, len, "%s", id_str.c_str());
    mvwprintw(info_win, start++, len, "%s", last_access.c_str());
    mvwprintw(info_win, start++, len, "%s", last_change.c_str());
    mvwprintw(info_win, start++, len, "%s", last_modification.c_str());
    mvwprintw(info_win, start++, len, "%s", inode_str.c_str());
    mvwprintw(info_win, start, len, "%s", file_system.c_str());
    wattroff(info_win, COLOR_PAIR(GREEN_COLOR));
    wrefresh(info_win);
    getch();
    werase(info_win);
    wrefresh(info_win);
    delwin(info_win);
}

void file_panel::set_current_directory(const std::string &_current_directory) {
    current_directory = _current_directory;
}

bool file_panel::is_active_panel() const {
    return active_panel;
}

void file_panel::set_current_ind(size_t _current_ind) {
    current_ind = _current_ind;
}

void file_panel::set_start_ind(size_t _start_ind) {
    start_index = _start_ind;
}

void file_panel::calculate_size() {
    uintmax_t size_bytes = 0;
    if (this->content[current_ind].content_type == CONTENT_TYPE::IS_DIR) {
        if (this->content[current_ind].name_content == "..") {
            return;
        }
        std::filesystem::path current_path(this->current_directory + "/" + this->content[current_ind].name_content);
        if ((status(current_path).permissions() & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
            std::string message = "Cannot calculate size from: '" + current_path.filename().string() + "'";
            create_error_panel(" Permission error ", message,
                               HEIGHT_FUNCTIONAL_PANEL - 2,
                               WEIGHT_FUNCTIONAL_PANEL > message.length()
                               ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
            return;
        }
        for (auto && entry : std::filesystem::recursive_directory_iterator(current_path, std::filesystem::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() || entry.is_regular_file()
            || entry.is_block_file() || entry.is_fifo() || entry.is_socket()) {
                struct stat sb {};
                lstat(entry.path().string().c_str(), &sb);
                size_bytes += sb.st_size;
            }
        }
        create_calculate_panel(size_bytes, current_path.filename().string());
    }
}

void filesystem_info_mount() {
    clear();
    refresh();
    WINDOW* win = newwin(LINES, COLS, 0, 0);
    wattron(win, COLOR_PAIR(10));
    mvwprintw(win, LINES - 1, 0, "%*s", COLS, " ");
    mvwprintw(win, 0, 0, "%*s", COLS, " ");
    std::string file_message = "Information about filesystem";
    std::string continue_message = "Press any button to continue";
    mvwprintw(win, 0, static_cast<int>((COLS - file_message.length()) / 2), "%s", file_message.c_str());
    mvwprintw(win, LINES - 1, static_cast<int>((COLS - continue_message.length()) / 2), "%s", continue_message.c_str());
    wattroff(win, COLOR_PAIR(10));


    std::vector<std::string> a;
    std::string command = "df -h";
    std::shared_ptr<FILE> pipe(popen(command.c_str(), "r"), pclose);
    char buffer[256];
    while(!feof(pipe.get())) {
        if ((fgets(buffer, 256, pipe.get())) != nullptr) {
            a.emplace_back(buffer);
        }
    }

    int start = static_cast<int>((LINES - a.size()) / 2);
    size_t max_len = 0;
    for (size_t i = 1; i < a.size(); i++) {
        if (a[i].length() > max_len) {
            max_len = a[i].length();
        }
    }

    int len = static_cast<int>((COLS - max_len) / 2);

    wattron(win, COLOR_PAIR(RED_COLOR));
    mvwprintw(win, start++, len, "%s", a[0].c_str());
    wattroff(win, COLOR_PAIR(RED_COLOR));
    wattron(win, COLOR_PAIR(GREEN_COLOR));
    for (size_t i = 1; i < a.size(); i++) {
        mvwprintw(win, start++, len, "%s", a[i].c_str());
    }
    wattroff(win, COLOR_PAIR(GREEN_COLOR));

    wrefresh(win);
    getch();
    werase(win);
    wrefresh(win);
    delwin(win);
}

bool is_input_field_find(size_t _index) {
    if (_index == 0 || _index == 7 || _index == 8) {
        return true;
    }
    return false;
}

void fill_permissions(char_permissions &perms, std::filesystem::perms& new_perms) {
    if (perms.owner_perm[0] == 'r') new_perms |= std::filesystem::perms::owner_read;
    if (perms.owner_perm[1] == 'w') new_perms |= std::filesystem::perms::owner_write;
    if (perms.owner_perm[2] == 'x') new_perms |= std::filesystem::perms::owner_exec;
    if (perms.group_perm[0] == 'r') new_perms |= std::filesystem::perms::group_read;
    if (perms.group_perm[1] == 'w') new_perms |= std::filesystem::perms::group_write;
    if (perms.group_perm[2] == 'x') new_perms |= std::filesystem::perms::group_exec;
    if (perms.other_perm[0] == 'r') new_perms |= std::filesystem::perms::others_read;
    if (perms.other_perm[1] == 'w') new_perms |= std::filesystem::perms::others_write;
    if (perms.other_perm[2] == 'x') new_perms |= std::filesystem::perms::others_exec;
}

void convert_str_to_window_size(std::string &_to, std::string &_from, size_t window_size) {
    size_t len = _from.length();
    for (size_t i = 0; i < static_cast<int>(window_size / 2) - 1; i++) {
        _to.push_back(_from[i]);
    }
    _to.push_back('~');
    for (size_t j = len - window_size / 2; j < len; j++) {
        _to.push_back(_from[j]);
    }
}

void create_help_menu(char& _choice) {
    int weight = 45;
    int height = 15;
    WINDOW* win = newwin(height, weight, (LINES - height) / 2, (COLS - weight) / 2);
    size_t current_ind = 0;
    size_t start = 0;
    wbkgd(win, COLOR_PAIR(12));
    help_menu_show_content(win, height, weight, start, current_ind);
    wrefresh(win);
    bool flag_continue = true;
    while(flag_continue) {
        switch(getch()) {
            case KEY_RESIZE : {
                flag_continue = false;
                break;
            }
            case KEY_DOWN : {
                help_panel_pagination(KEY_DOWN, height, start, current_ind);
                help_menu_show_content(win, height, weight, start, current_ind);
                break;
            }
            case KEY_UP : {
                help_panel_pagination(KEY_UP, height, start, current_ind);
                help_menu_show_content(win, height, weight, start, current_ind);
                break;
            }
            case 'q' : {
                flag_continue = false;
                break;
            }
        }
    }
    delwin(win);
}

void help_menu_show_content(WINDOW *_win, size_t _height, size_t _weight, size_t _start, size_t _current_ind) {
    werase(_win);
    refresh();
    box(_win, 0, 0);
    wattron(_win, A_BOLD);
    mvwprintw(_win, 0, static_cast<int>(_weight - (strlen(" Help menu "))) / 2, "%s", " Help menu ");
    mvwprintw(_win, static_cast<int>(_height - 1), static_cast<int>((_weight - strlen("Exit[q]")) / 2), "%s", "Exit[q]");
    size_t offset = 1;
    for (size_t i = _start; i < help_vec.size() && i < (_height - 2) + _start; i++) {
        if (_current_ind == i) {
            wattron(_win, A_REVERSE);
        }
        mvwprintw(_win, static_cast<int>(offset), 1, "%*s", static_cast<int>(_weight - 2), " ");
        mvwprintw(_win, static_cast<int>(offset), 1, "%s", help_vec[i].first.c_str());
        mvwprintw(_win, static_cast<int>(offset), 9, "%s", help_vec[i].second.c_str());

        wattroff(_win, A_REVERSE);
        offset++;
    }
    refresh_sub_panel(_win);
    wattroff(_win, A_BOLD);
}

void help_panel_pagination(size_t _direction, size_t _height, size_t &_start, size_t &_current_ind) {
    if (_direction == KEY_UP) {
        if (_current_ind == 0) {
            _start = 0;
        } else if (_current_ind != _start) {
            _current_ind--;
        } else {
            _start -= _height - 2;
            _current_ind--;
        }
    } else if (_direction == KEY_DOWN) {
        if (_current_ind + 1 < help_vec.size()) {
            if (_current_ind + 1 >= _height - 2 + _start) {
                _start += _height - 2;
            }
            _current_ind++;
        }
    }
}

void create_calculate_panel(uintmax_t size, const std::string& filename) {
    int height = 7;
    int weight = 40;
    WINDOW* win = newwin(height, weight, (LINES - height) / 2, (COLS - weight) / 2);
    wbkgd(win, COLOR_PAIR(12));
    box(win, 0, 0);
    mvwprintw(win, 0, static_cast<int>((weight - strlen("Calculate size")) / 2), "%s", "Calculate size");
    mvwprintw(win, height - 1, static_cast<int>((weight - strlen("Press any button to continue")) / 2), "%s", "Press any button to continue");
    mvwprintw(win, 2, 3, "%s%s","File: ", filename.c_str());
    mvwprintw(win, 3, 3, "%s%zu%s", "Size: ", size, " bytes");
    wrefresh(win);
    getch();
    delwin(win);
}
