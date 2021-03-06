#include "tool_fix.hpp"
#include "core_board.hpp"
#include <iostream>

namespace horizon {

ToolFix::ToolFix(Core *c, ToolID tid) : ToolBase(c, tid)
{
}

bool ToolFix::can_begin()
{
    for (const auto &it : core.r->selection) {
        if (it.type == ObjectType::BOARD_PACKAGE) {
            auto pkg = &core.b->get_board()->packages.at(it.uuid);
            if (pkg->fixed == (tool_id == ToolID::UNFIX))
                return true;
        }
    }
    return false;
}

ToolResponse ToolFix::begin(const ToolArgs &args)
{
    for (const auto &it : args.selection) {
        if (it.type == ObjectType::BOARD_PACKAGE) {
            core.b->get_board()->packages.at(it.uuid).fixed = tool_id == ToolID::FIX;
        }
    }
    core.r->commit();
    return ToolResponse::end();
}
ToolResponse ToolFix::update(const ToolArgs &args)
{
    return ToolResponse();
}
} // namespace horizon
