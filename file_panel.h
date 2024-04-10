#ifndef COURSE_PROJECT_FILE_PANEL_H
#define COURSE_PROJECT_FILE_PANEL_H

#include <string>
#include <vector>
#include <panel.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctime>
#include <algorithm>
#include <form.h>

#define DATE_LEN 16
#define LEN_LINE_FIRST 30
#define MAX_SIZE_LEN 11
#define SIZE_FIELD_BUFFER_1 5
#define HEADER_NAME "Name"
#define HEADER_SIZE "Size"
#define HEADER_MODIFY_DATE "Modify date"
#define HEADER_CREATE_DIR " Create symlink "
#define OK_BUTTON "[ OK ]"
#define NO_BUTTON "[ NO ]"
#define LINK_NAME_HEADER "Link name:"
#define LINK_POINTER_HEADER "Pointing to:"

enum class CONTENT_TYPE {
    IS_DIR = 0,
    IS_LNK = 1,
    IS_REG = 2,
};

struct info {
    std::string name_content;
    std::string last_redact_content;
    CONTENT_TYPE content_type;
    ssize_t size_content;

    info(std::string_view _name_content,
         std::string_view _last_redact_content,
         ssize_t _current_ind, CONTENT_TYPE _content_type);
};

class file_panel {
private:
    std::string current_directory;
    WINDOW* win;
    std::vector<info> content;
    bool active_panel;
    PANEL* panel;
    size_t start_index;
    size_t current_ind;
public :
    file_panel() = delete;
    ~file_panel();
    explicit file_panel(std::string_view _current_directory, size_t _rows,
                        size_t _cols, size_t _x, size_t _y);
    [[nodiscard]] const std::vector<info>& get_content() const;
    [[nodiscard]] PANEL* get_panel() const;
    [[nodiscard]] size_t get_current_ind() const;
    void display_box();
    void display_current_dir();
    void set_active_panel(bool _active_panel);
    void move_cursor_and_pagination(size_t _direction);
    void display_headers();
    void read_current_dir();
    void display_lines();
    void switch_directory(const std::string& _direction);
    void refresh_panels();
    void resize_panel(size_t _rows, size_t _cols, size_t _x, size_t _y);
    void display_content();
};


void init_colors();
void move_cursor_right_from_input_field(size_t len, size_t* _current_index, int* _current_offset_field);
void move_cursor_left_from_input_field(size_t* _current_index, int* _current_offset_field);
void insert_char_from_input_field(std::string& _current_buffer, size_t* _current_index, int* _current_offset_field, int ch);
void delete_char_from_input_field(std::string& _current_buffer, size_t* _current_index, int* _offset_field);
bool is_input_field(size_t _index);
void create_functional_panel();
void navigation_from_functional_panel(WINDOW* _win, FORM* _form, FIELD** _fields);
void display_buffer_on_form(FORM* _form, const std::string& _buffer, const size_t* _ind, int _offset);


#endif //COURSE_PROJECT_FILE_PANEL_H
