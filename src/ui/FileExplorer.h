#pragma once

namespace vrutti::core::fs {
    class Workspace;
    struct FileNode;
}

namespace vrutti::ui {

    class FileExplorer {
    public:
        void render(vrutti::core::fs::Workspace& workspace);
    private:
        void renderNode(vrutti::core::fs::Workspace& workspace, vrutti::core::fs::FileNode& node);
    };

}