#pragma once
#include <vector>
#include <map>

struct InputEvent {
    union {
        struct {
            float x;
            float y;
        } click;
        struct {
            std::string* fn;
        } state;
    };
    enum Type { CLICK, SAVE, LOAD } type;

    InputEvent() : type(CLICK) {
        click.x = click.y = 0.f;
    }

    InputEvent(const std::string& filename, Type tp) : type(tp) {
        state.fn = new std::string(filename);
    }

    ~InputEvent() {
        if (type == SAVE || type == LOAD)
            delete state.fn;
    }

    InputEvent(const InputEvent& other) : type(other.type) {
        if (type == SAVE || type == LOAD)
            state.fn = new std::string(*other.state.fn);
        else
            click = other.click;
    }

    InputEvent& operator=(const InputEvent& other) {
        if (this == &other) return *this;
        if (type == SAVE || type == LOAD) delete state.fn;
        type = other.type;
        if (type == SAVE || type == LOAD)
            state.fn = new std::string(*other.state.fn);
        else
            click = other.click;
        return *this;
    }
};

void input_tick();
void input_init();
