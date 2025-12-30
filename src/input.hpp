#pragma once
#include <vector>
#include <map>

static const int keys_to_check[] = {
    VK_LCONTROL, VK_RCONTROL, VK_RETURN, '0', '1', '2',
    '3', '4', '5', '6', '7', '8', '9', VK_DECIMAL, VK_OEM_COMMA, VK_OEM_PERIOD,
    VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_LSHIFT, VK_RSHIFT, VK_BACK, VK_SPACE, VK_DELETE
};

struct InputEvent {
    union {
        struct {
            float x;
            float y;
        } click;
        struct {
            std::string fn;
        } state;
    };
    enum Type { CLICK, SAVE, LOAD } type;

    InputEvent() : type(CLICK), click{ 0.0f, 0.0f } {}
    InputEvent(const std::string& filename, Type tp) : type(tp) {
        new (&state.fn) std::string(filename);
    }

    ~InputEvent() {
        if (type == SAVE || type == LOAD) {
            state.fn.~basic_string();
        }
    }

    InputEvent(const InputEvent& other) : type(other.type) {
        if (type == SAVE || type == LOAD) {
            new (&state.fn) std::string(other.state.fn);
        }
        else {
            click = other.click;
        }
    }

    InputEvent& operator=(const InputEvent& other) {
        if (this == &other) return *this;
        if ((type == SAVE || type == LOAD) && other.type != SAVE && other.type != LOAD) {
            state.fn.~basic_string();
        }
        if (other.type == SAVE || other.type == LOAD) {
            if (type == SAVE || type == LOAD) state.fn = other.state.fn;
            else new (&state.fn) std::string(other.state.fn);
        }
        else {
            click = other.click;
        }

        type = other.type;
        return *this;
    }
};

void input_tick();
void input_init();
