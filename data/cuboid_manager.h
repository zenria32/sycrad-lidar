#pragma once

#include "cuboid.h"

#include <QObject>
#include <QUndoStack>

#include <cstdint>
#include <vector>

class cuboid_manager : public QObject {
    Q_OBJECT

    public:
    explicit cuboid_manager(QObject *parent = nullptr);
    ~cuboid_manager();

    uint32_t add_cuboid(const cuboid &c);
    void remove_cuboid(uint32_t id);
    void update_cuboid(uint32_t id, const cuboid &updated);

    void update_cuboid_avoid_undo_stack(uint32_t id, const cuboid &updated);
    void update_cuboid_with_undo_stack(uint32_t id, const cuboid &previous_state);

    cuboid *find(uint32_t id);
    const cuboid *find(uint32_t id) const;

    const std::vector<cuboid> &get_cuboids() const { return cuboids; }
    std::size_t count() const { return cuboids.size(); }

    void select(uint32_t id);
    void deselect();

    uint32_t get_selected_id() const { return selected_id; }
    const cuboid *get_selected_cuboid() const;
    bool has_anything_selected() const { return selected_id != 0; }

    QUndoStack *get_undo_stack() { return &undo_stack; }

    void clear();

    signals:
    void cuboid_added(uint32_t id);
    void cuboid_removed(uint32_t id);
    void cuboid_updated(uint32_t id);
    void selected_cuboid_changed(uint32_t previous_id, uint32_t new_id);
    void cleared();

    private:
    friend class add_cuboid_command;
    friend class remove_cuboid_command;
    friend class update_cuboid_command;

    uint32_t create_cuboid(cuboid &c);
    void delete_cuboid(uint32_t id);
    void update_requested(uint32_t id, const cuboid &c);

    std::vector<cuboid> cuboids;
    uint32_t selected_id = 0;
    uint32_t next_id = 1;

    QUndoStack undo_stack;
};