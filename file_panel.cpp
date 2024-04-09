#include <cstring>
#include "file_panel.h"

void file_panel::read_current_dir() {
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
            buffer.insert(0, 1, '/');
            content_type = CONTENT_TYPE::IS_DIR;
        } else if ((st.st_mode & S_IFMT) == S_IFLNK) {
            content_type = CONTENT_TYPE::IS_LNK;
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
        if (i == current_ind && active_panel) {
            wattron(win, A_REVERSE);
        }
        mvwprintw(win, static_cast<int>(ind_offset), 1, "%*s", (COLS / 2) - 2, " ");

        if (content[i].content_type == CONTENT_TYPE::IS_DIR
            && ((current_ind != ind_offset - 2 + start_index) || !active_panel)) {
            wattron(win, COLOR_PAIR(3));
        }

        size_t len_line = COLS / 2 - DATE_LEN - MAX_SIZE_LEN - 1;
        if (len_line > content[i].name_content.length()) {
            mvwprintw(win, static_cast<int>(ind_offset), 1, "%s", content[i].name_content.c_str());
        } else {
            mvwprintw(win, static_cast<int>(ind_offset), 1, "%s",
                      content[i].name_content.substr(0, len_line).c_str());
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
    if (_direction == "/..") {
        if (current_directory == "/")
            return;
        new_current_directory = current_directory.substr(0, current_directory.find_last_of('/'));
        if (new_current_directory.empty()) {
            new_current_directory = "/";
        }
        d = opendir(new_current_directory.c_str());
    } else {
        if (current_directory == "/") {
            current_directory.pop_back();
        }
        new_current_directory = current_directory + _direction;
        d = opendir(new_current_directory.c_str());
    }
    if (d == nullptr) {
        return;
    }
    current_directory = new_current_directory;
    start_index = 0;
    current_ind = 0;
    content.clear();
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


info::info(std::string_view _name_content,
           std::string_view _last_redact_content,
           ssize_t _current_ind, CONTENT_TYPE _content_type) {
    this->content_type = _content_type;
    this->name_content = _name_content;
    this->last_redact_content = _last_redact_content;
    this->size_content = _current_ind;
}

void create_functional_panel() {
    int height = 10;
    int weight = 60;
    WINDOW* win = newwin(height, weight, (LINES - height) / 2, (COLS - weight) / 2);
    box(win, 0, 0);
    wbkgd(win, COLOR_PAIR(4));
    wattron(win, COLOR_PAIR(5));
    wattron(win, A_BOLD);
    mvwprintw(win, 0, (weight - static_cast<int>(strlen(HEADER_CREATE_DIR))) / 2, "%s", HEADER_CREATE_DIR);
    wattroff(win, A_BOLD);
    wattroff(win, COLOR_PAIR(5));

    wbkgd(win, COLOR_PAIR(4));
    wrefresh(win);

    curs_set(1);

    FIELD *fields[SIZE_FIELD_BUFFER_1];
    fields[0] = new_field(1, LEN_LINE_FIRST, 1, 15, 0, 0);
    fields[1] = new_field(1, LEN_LINE_FIRST, 3, 15, 0, 0);
    fields[2] = new_field(1, static_cast<int>(strlen(OK_BUTTON)), 5, 20, 0, 0);
    fields[3] = new_field(1, static_cast<int>(strlen(NO_BUTTON)), 5, 34, 0, 0);
    fields[4] = nullptr;

    set_field_back(fields[0], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[1], COLOR_PAIR(6) | A_BOLD);
    set_field_back(fields[2], COLOR_PAIR(7) | A_BOLD);
    set_field_back(fields[3], COLOR_PAIR(7) | A_BOLD);
    set_field_buffer(fields[2], 0, OK_BUTTON);
    set_field_buffer(fields[3], 0, NO_BUTTON);
    FORM *my_form = new_form(fields);
    set_form_win(my_form, win);

    WINDOW* subwin = derwin(win, height - 4, weight - 2, 2, 1);
    set_form_sub(my_form, subwin);
    post_form(my_form);

    wrefresh(subwin);

    wrefresh(win);
    pos_form_cursor(my_form);
    wrefresh(win);

    navigation_from_functional_panel(win, my_form);

    for (size_t i = 0; i < SIZE_FIELD_BUFFER_1 - 1; i++) {
        free_field(fields[i]);
    }
    free_form(my_form);
    delwin(subwin);
    delwin(win);
    curs_set(0);
}

void navigation_from_functional_panel(WINDOW* _win, FORM* _form) {
    int ch;
    std::string first_field_buffer;
    std::string second_field_buffer;
    std::string* current_buffer = &first_field_buffer;
    int current_num_field = 0;
    size_t index_first_field = 0;
    size_t index_second_field = 0;
    int offset_first_field = 0;
    int offset_second_field = 0;
    int* current_offset_field = &offset_first_field;
    size_t* current_index = &index_first_field;
    while((ch = getch()) != '\n' && ch != KEY_RESIZE) {
        switch(ch) {
            case '\t' : {
                if (current_num_field == 0) {
                    current_index = &index_second_field;
                    current_num_field = 1;
                    current_buffer = &second_field_buffer;
                    current_offset_field = &offset_second_field;
                } else {
                    current_index = &index_first_field;
                    current_num_field = 0;
                    current_buffer = &first_field_buffer;
                    current_offset_field = &offset_first_field;
                }
                form_driver(_form, REQ_NEXT_FIELD);
                break;
            }
            case KEY_LEFT : {
                if (*current_index != 0) {
                    (*current_index)--;
                }
                if (*current_offset_field == *current_index && *current_index != 0) {
                    (*current_offset_field)--;
                }
                break;
            }
            case KEY_RIGHT : {
                if (*current_index >= current_buffer->length()) {
                        current_buffer->push_back(' ');
                }
                (*current_index)++;
                if (*current_index > LEN_LINE_FIRST - 1 + *current_offset_field) {
                    (*current_offset_field)++;
                }
                break;
            }
            case KEY_BACKSPACE : {
                if (!current_buffer->empty() && *current_index != 0) {
                    current_buffer->erase((*current_index) - 1, 1);
                    (*current_index)--;
                }
                if (*current_offset_field != 0) {
                    (*current_offset_field)--;
                }
                break;
            }
            default: {
                current_buffer->insert(current_buffer->begin() + (*current_index), static_cast<char>(ch));
                (*current_index)++;
                if (*current_index > LEN_LINE_FIRST - 1 + *current_offset_field) {
                    (*current_offset_field)++;
                }
                break;
            }
        }
        display_buffer_on_form(_form, *current_buffer, current_index, *current_offset_field);
        wrefresh(_win);
    }
}

void display_buffer_on_form(FORM* _form, const std::string& _buffer, const size_t* _ind, int _offset) {
    if (_buffer.length() < LEN_LINE_FIRST - 1 + _offset) {
        set_field_buffer(current_field(_form), 0, _buffer.c_str());
    } else {
        set_field_buffer(current_field(_form), 0, _buffer.substr(_offset, LEN_LINE_FIRST - 1).c_str());
    }
    for (size_t i = 0; i < *_ind - _offset; i++) {
        form_driver(_form, REQ_RIGHT_CHAR);
    }
}
