#pragma once

#include "data.h"
#include <cuboid.h>
#include <cuboid_manager.h>

#include <QUndoCommand>

class add_cuboid_command : public QUndoCommand {
    public:
    add_cuboid_command(cuboid_manager *manager, const cuboid &c) : QUndoCommand("Add Cuboid"), mngr(manager), data(c) {
        assigned_id = mngr->create_cuboid(data);
        data.id = assigned_id;
    }

    void undo() override { mngr->delete_cuboid(assigned_id); }
    void redo() override {
        if (first_redo) {
            first_redo = false;
            return;
        }
        data.id = assigned_id;
        mngr->cuboids.push_back(data);
        emit mngr->cuboid_added(assigned_id);
    }

    uint32_t get_cuboid_id() const { return assigned_id; }

    private:
    cuboid_manager *mngr;
    cuboid data;
    uint32_t assigned_id;
    bool first_redo = true;
};

class remove_cuboid_command : public QUndoCommand {
    public:
    remove_cuboid_command(cuboid_manager *manager, const cuboid &c) : QUndoCommand("Remove Cuboid"), mngr(manager), data(c) {
        mngr->delete_cuboid(data.id);
    }

    void undo() override {
        mngr->cuboids.push_back(data);
        emit mngr->cuboid_added(data.id);
    }

    void redo() override {
        if (first_redo) {
            first_redo = false;
            return;
        }
        mngr->delete_cuboid(data.id);
    }

    private:
    cuboid_manager *mngr;
    cuboid data;
    bool first_redo = true;
};

class update_cuboid_command : public QUndoCommand {
    public:
    update_cuboid_command(cuboid_manager *manager, const cuboid &previous, const cuboid &updated) : QUndoCommand
    ("Update Cuboid"), mngr(manager), previous_data(previous), new_data(updated) {
        new_data.id = previous.id;
        mngr->update_requested(new_data.id, new_data);
    }

    void undo() override { mngr->update_requested(previous_data.id, previous_data); }
    void redo() override {
        if (first_redo) {
            first_redo = false;
            return;
        }
        mngr->update_requested(new_data.id, new_data);
    }

    private:
    cuboid_manager *mngr;
    cuboid previous_data;
    cuboid new_data;
    bool first_redo = true;
};