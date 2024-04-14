#include <cstring>
#include "file_panel.h"

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
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            content_type = CONTENT_TYPE::IS_DIR;
        } else if ((st.st_mode & S_IFMT) == S_IFLNK) {
            bool flag_dir = false;
            char target_path[PATH_MAX];
            ssize_t len = readlink(full_path.c_str(), target_path, sizeof(target_path));
            if (len != -1) {
                target_path[len] = '\0';
                lstat(target_path, &st);
                if ((st.st_mode & S_IFMT) == S_IFDIR) {
                    content_type = CONTENT_TYPE::IS_LNK_TO_REG;
                    flag_dir = true;
                }
            }
            if (!flag_dir) {
                content_type = CONTENT_TYPE::IS_LNK;
            }
        } else if ((st.st_mode & S_IFMT) == S_IFREG) {
            content_type = CONTENT_TYPE::IS_REG;
        }
        content.emplace_back(buffer, date, size, content_type);
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
            mvprintw(LINES - 1, 0, "%s%zu%s%zu%s%s", "File:     ",
                     current_ind + 1, " of ", content.size(), "     Path: ",
                     (current_directory + "/" + content[current_ind].name_content).c_str());
            attroff(A_BOLD | COLOR_PAIR(10));
        }
        if (i == current_ind && active_panel) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, static_cast<int>(ind_offset), 1, "%*s", (COLS / 2) - 2, " ");

        if ((content[i].content_type == CONTENT_TYPE::IS_DIR || content[i].content_type == CONTENT_TYPE::IS_LNK_TO_REG)
            && ((current_ind != ind_offset - 2 + start_index) || !active_panel)) {
            wattron(win, COLOR_PAIR(3));
        }
        std::string output_string = content[i].name_content;
        convert_to_output(output_string, content[i].content_type);
        size_t len_line = COLS / 2 - DATE_LEN - MAX_SIZE_LEN - 1;
        if (len_line > output_string.length()) {
            mvwprintw(win, static_cast<int>(ind_offset), 1, "%s", output_string.c_str());
        } else {
            mvwprintw(win, static_cast<int>(ind_offset), 1, "%s",
                      output_string.substr(0, len_line).c_str());
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
        new_current_directory = current_directory + "/" + _direction;
        d = opendir(new_current_directory.c_str());
    }
    std::filesystem::path check_perm(new_current_directory);

    if ((std::filesystem::status(check_perm).permissions() & std::filesystem::perms::owner_read) ==
        std::filesystem::perms::none
        || (std::filesystem::status(check_perm).permissions() & std::filesystem::perms::owner_exec) ==
           std::filesystem::perms::none) {
        std::string message = "Cannot chdir to " + _direction;
        int size = static_cast<int>(message.length());
        create_error_panel(" Permission error ",
                           message, 8,
                           WEIGHT_FUNCTIONAL_PANEL > size ? WEIGHT_FUNCTIONAL_PANEL : size + 2);
        return;
    }
    if (d == nullptr) {
        return;
    }
    start_index = 0;
    current_ind = 0;
    current_directory = new_current_directory;
    read_current_dir();
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
    mvwprintw(win, 0, 2, "%s", current_directory.c_str());
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
                    std::filesystem::rename(dir_path / content[current_ind].name_content, dir_path / new_name);
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
                               WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL : message.length() +
                                                                                                      2);
            return;
        }
        if (exists(dir_path)) {
            if (!exists(dir_path / name_directory)) {
                std::filesystem::create_directory(dir_path / name_directory);
                if (_other_panel.current_directory == this->current_directory) {
                    _other_panel.read_current_dir();
                }
                read_current_dir();
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
                               WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL : message.length() +
                                                                                                      2);

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
                               WEIGHT_FUNCTIONAL_PANEL > message.length() ? WEIGHT_FUNCTIONAL_PANEL : message.length() + 2);
            return;
        }
        if (exists(dir_path)) {
            if (!exists(dir_path / namelink) && exists(other_panel_path / pointing_to)) {
                std::filesystem::create_symlink(other_panel_path / pointing_to, dir_path / namelink);
                if (_other_panel.current_directory == this->current_directory) {
                    this->read_current_dir();
                }
                _other_panel.read_current_dir();
            }
        }
    }

}

void file_panel::create_hardlink(file_panel &_other_panel) {
    std::string namelink;
    std::string pointing_to = _other_panel.content[_other_panel.current_ind].name_content;
    bool entry_flag = symlink_hardlink_func_panel(HEADER_CREATE_HARDLINK, namelink, pointing_to,
                                                  HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);
}

void file_panel::copy_content(file_panel &_other_panel) {
    if (content[current_ind].name_content != "/..") {
        std::string path = _other_panel.current_directory;
        bool entry_flag = create_redact_other_func_panel(HEADER_COPY, "Copy '" + content[current_ind]
                                                                 .name_content + "' to::", _other_panel.current_directory,
                                                         HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);
        if (entry_flag) {
            std::filesystem::path copy_path_from(current_directory + "/" + content[current_ind].name_content);
            std::filesystem::path copy_path_to(path);
            if (exists(copy_path_to)) {
                if (content[current_ind].content_type == CONTENT_TYPE::IS_DIR) {
                    std::filesystem::copy(copy_path_from, copy_path_to, std::filesystem::copy_options::recursive);
                } else if (content[current_ind].content_type == CONTENT_TYPE::IS_REG) {
                    std::filesystem::copy_file(copy_path_from, copy_path_to);
                } else if (content[current_ind].content_type == CONTENT_TYPE::IS_LNK
                           || content[current_ind].content_type == CONTENT_TYPE::IS_LNK_TO_REG) {
                    std::filesystem::copy_symlink(copy_path_from, copy_path_to);
                }
            }
        }
    }
}

void file_panel::move_content(std::string other_panel_path) {
    if (content[current_ind].name_content != "/..") {
        create_redact_other_func_panel(HEADER_MOVE, "Move '" + content[current_ind]
                                               .name_content + "' to::", other_panel_path,
                                       HEIGHT_FUNCTIONAL_PANEL, WEIGHT_FUNCTIONAL_PANEL);
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
        if (perms.owner_perm[0] == 'r') new_perms |= std::filesystem::perms::owner_read;
        if (perms.owner_perm[1] == 'w') new_perms |= std::filesystem::perms::owner_write;
        if (perms.owner_perm[2] == 'x') new_perms |= std::filesystem::perms::owner_exec;
        if (perms.group_perm[0] == 'r') new_perms |= std::filesystem::perms::group_read;
        if (perms.group_perm[1] == 'w') new_perms |= std::filesystem::perms::group_write;
        if (perms.group_perm[2] == 'x') new_perms |= std::filesystem::perms::group_exec;
        if (perms.other_perm[0] == 'r') new_perms |= std::filesystem::perms::others_read;
        if (perms.other_perm[1] == 'w') new_perms |= std::filesystem::perms::others_write;
        if (perms.other_perm[2] == 'x') new_perms |= std::filesystem::perms::others_exec;
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
    std::filesystem::path p(current_directory + "/" + content[current_ind].name_content);
    REMOVE_TYPE type;
    bool flag_permission_read = false;
    try {
        flag_permission_read = std::filesystem::is_empty(p);
    } catch (std::filesystem::filesystem_error &e) {
        int error_ind = e.code().value();
        if (error_ind == 13) {
            generate_permission_error(e);
        }
    }
    if (is_directory(p) && !flag_permission_read) {
        std::stack<std::filesystem::path> dir_stack;
        dir_stack.push(p);
        while (!dir_stack.empty()) {
            std::filesystem::path current_path = dir_stack.top();
            dir_stack.pop();

            for (const auto &entry: std::filesystem::directory_iterator(current_path)) {
                size_t ind = entry.path().string().find(content[current_ind].name_content);
                std::string message = "Delete: " + entry.path().string().substr(ind);
                type = create_remove_panel("Delete: " + message, HEIGHT_FUNCTIONAL_PANEL - 2,
                                           WEIGHT_FUNCTIONAL_PANEL);

                try {
                    if (type == REMOVE_TYPE::REMOVE_ALL) {
                        std::filesystem::remove_all(p);
                    } else if (type == REMOVE_TYPE::REMOVE_THIS) {
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
                if (type == REMOVE_TYPE::REMOVE_ALL) {
                    read_current_dir();
                    if (_other_panel.current_directory == current_directory) {
                        _other_panel.read_current_dir();
                    }
                    return;
                } else if (type == REMOVE_TYPE::STOP_REMOVE) {
                    return;
                }
                if (std::filesystem::is_directory(entry.path())) {
                    dir_stack.push(entry.path());
                }
            }
        }
    } else {
        type = create_remove_panel("Delete: " + content[current_ind].name_content, HEIGHT_FUNCTIONAL_PANEL - 2,
                                   WEIGHT_FUNCTIONAL_PANEL);
        if (type == REMOVE_TYPE::REMOVE_ALL || type == REMOVE_TYPE::REMOVE_THIS) {
            try {
                std::filesystem::remove(p);
                read_current_dir();
                if (_other_panel.current_directory == current_directory) {
                    _other_panel.read_current_dir();
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


void generate_permission_error(std::filesystem::filesystem_error &e) {
//    size_t ind = e.path1().string().find(find_str);
//    std::string message = func + e.path1().string().substr(ind);
    std::string message = e.path1().string();
    int len = static_cast<int>(message.length());
    create_error_panel(" Permission error ", message,
                       HEIGHT_FUNCTIONAL_PANEL - 2,
                       WEIGHT_FUNCTIONAL_PANEL > len ? WEIGHT_FUNCTIONAL_PANEL : len + 2);
}

info::info(std::string_view _name_content,
           std::string_view _last_redact_content,
           ssize_t _current_ind,
           CONTENT_TYPE _content_type) {
    this->content_type = _content_type;
    this->name_content = _name_content;
    this->last_redact_content = _last_redact_content;
    this->size_content = _current_ind;
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

    bool entry_flag = navigation_symlink_hardlink_create_panel(win, my_form, fields, _namelink, _pointer);

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

bool navigation_symlink_hardlink_create_panel(WINDOW *_win, FORM *_form, FIELD **_fields, std::string &_namelink,
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
    display_buffer_on_form(_form, _pointer, &index_second_field, offset_second_field);
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
                                                 current_offset_field, ch);
                }
                break;
            }
        }
        if (is_input_field_link(index_field)) {
            display_buffer_on_form(_form, *current_buffer, current_index,
                                   *current_offset_field);
        }
        wrefresh(_win);
    }
}

void display_buffer_on_form(FORM *_form, const std::string &_buffer,
                            const size_t *_ind, int _offset) {
    if (_buffer.length() < LEN_LINE_FIRST - 1 + _offset) {
        set_field_buffer(current_field(_form), 0, _buffer.c_str());
    } else {
        set_field_buffer(current_field(_form), 0, _buffer.substr(_offset, LEN_LINE_FIRST - 1).c_str());
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
                                  int ch) {
    _current_buffer.insert(_current_buffer.begin() + static_cast<long>(*_current_index), static_cast<char>(ch));
    (*_current_index)++;
    if (*_current_index > LEN_LINE_FIRST - 1 + *_current_offset_field) {
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

REMOVE_TYPE create_remove_panel(const std::string &description, int _height, int _weight) {
    WINDOW *win = create_functional_panel(HEADER_DELETE, _height, _weight);
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

    bool entry_flag = navigation_symlink_edit_permissions(win, my_form, fields, _perms);

    for (size_t i = 0; i < SIZE_FIELD_BUFFER_3 - 1; i++) {
        free_field(fields[i]);
    }

    free_form(my_form);
    delwin(subwin);
    delwin(win);
    return entry_flag;
}

bool navigation_symlink_edit_permissions(WINDOW *_win, FORM *_form, FIELD **_fields, char_permissions &_perms) {
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
        display_buffer_on_form(_form, _result, &current_index, offset);
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
                    insert_char_from_input_field(_result, &current_index, &offset, ch);
                }
            }
        }
        if (is_input_field_dir_file(index_field)) {
            display_buffer_on_form(_form, _result, &current_index, offset);
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
        case CONTENT_TYPE::IS_LNK_TO_REG : {
            _name.insert(0, 1, '~');
            break;
        }
        default :
            break;
    }
}
