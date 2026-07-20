#pragma once

#include "FileExplorer.h"
#include "EditorView.h"
#include "../../core/fs/Workspace.h"

namespace vrutti::ui::views {

    class Layout {
    public:
        Layout(core::fs::Workspace& workspace);
        void render();

    private:
        core::fs::Workspace& m_workspace;
        FileExplorer m_explorer;
        EditorView m_editor;
    };

}
