#pragma once

#include <memory>

namespace osc { class UndoRedoBase; }

namespace osc
{
    // a user-visible button, with a history dropdown menu, that performs an undo operation
    class UndoButton final {
    public:
        explicit UndoButton(std::shared_ptr<UndoRedoBase>);
        UndoButton(UndoButton const&) = delete;
        UndoButton(UndoButton&&) noexcept = default;
        UndoButton& operator=(UndoButton const&) = delete;
        UndoButton& operator=(UndoButton&&) noexcept = default;
        ~UndoButton() noexcept;

        void onDraw();
    private:
        std::shared_ptr<UndoRedoBase> m_UndoRedo;
    };
}
