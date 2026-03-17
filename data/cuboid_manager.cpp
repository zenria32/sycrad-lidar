#include "cuboid_manager.h"
#include "cuboid_commands.h"

#include <algorithm>

cuboid_manager::cuboid_manager(QObject *parent) : QObject(parent) {}
cuboid_manager::~cuboid_manager() = default;

uint32_t cuboid_manager::add_cuboid(const cuboid &c) {
    auto *command = new add_cuboid_command(this, c);
    undo_stack.push(command);
    return command->get_cuboid_id();
}

void cuboid_manager::remove_cuboid(uint32_t id) {
    const cuboid *existing = find(id);

    if (!existing) {
        return;
    }

    undo_stack.push(new remove_cuboid_command(this, *existing));
}

void cuboid_manager::update_cuboid(uint32_t id, const cuboid &updated) {
    const cuboid *existing = find(id);
    if (!existing) {
        return;
    }
    undo_stack.push(new update_cuboid_command(this, *existing, updated));
}

void cuboid_manager::update_cuboid_avoid_undo_stack(uint32_t id, const cuboid &updated) {
    update_requested(id, updated);
}

void cuboid_manager::update_cuboid_with_undo_stack(uint32_t id, const cuboid &previous_state) {
    const cuboid *current = find(id);
    if (!current) {
        return;
    }
    undo_stack.push(new update_cuboid_command(this, previous_state, *current));
}

cuboid *cuboid_manager::find(uint32_t id) {
    auto iterator = std::ranges::find_if(cuboids,[id](const cuboid &c) { return c.id == id; });
    return (iterator != cuboids.end()) ? &(*iterator) : nullptr;
}

const cuboid *cuboid_manager::find(uint32_t id) const {
    auto iterator = std::ranges::find_if(cuboids, [id](const cuboid &c) { return c.id == id;});
    return (iterator != cuboids.end()) ? &(*iterator) : nullptr;
}

void cuboid_manager::select(uint32_t id) {
    if (id == selected_id) {
        return;
    }
    if (id != 0 && !find(id)) {
        return;
    }

    const uint32_t previous_id = selected_id;
    selected_id = id;
    emit selected_cuboid_changed(previous_id, id);
}

void cuboid_manager::deselect() {
    select(0);
}

const cuboid *cuboid_manager::get_selected_cuboid() const {
    if (selected_id == 0) {
        return nullptr;
    }

    return find(selected_id);
}

void cuboid_manager::clear() {
    const uint32_t previous_id = selected_id;
    cuboids.clear();
    selected_id = 0;
    next_id = 1;
    undo_stack.clear();
    if (previous_id != 0) {
        emit selected_cuboid_changed(previous_id, selected_id);
        emit cleared();
    }
}

uint32_t cuboid_manager::create_cuboid(cuboid &c) {
    c.id = next_id++;
    cuboids.push_back(c);
    emit cuboid_added(c.id);
    return c.id;
}

void cuboid_manager::delete_cuboid(uint32_t id) {
    if (selected_id == id) {
        deselect();
    }

    auto iterator = std::ranges::find_if(cuboids, [id](const cuboid &c) { return c.id == id; });
    if (iterator != cuboids.end()) {
        cuboids.erase(iterator);
        emit cuboid_removed(id);
    }
}

void cuboid_manager::update_requested(uint32_t id, const cuboid &c) {
    cuboid *existing = find(id);
    if (!existing) {
        return;
    }

    existing->class_name = c.class_name;
    existing->position = c.position;
    existing->rotation = c.rotation;
    existing->dimension = c.dimension;
    existing->truncation = c.truncation;
    existing->occlusion = c.occlusion;
    existing->alpha = c.alpha;

    emit cuboid_updated(id);
}