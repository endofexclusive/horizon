#include "imp_symbol.hpp"
#include "canvas/canvas_gl.hpp"
#include "header_button.hpp"
#include "pool/part.hpp"
#include "symbol_preview/symbol_preview_window.hpp"
#include "widgets/unplaced_box.hpp"

namespace horizon {
ImpSymbol::ImpSymbol(const std::string &symbol_filename, const std::string &pool_path)
    : ImpBase(pool_path), core_symbol(symbol_filename, *pool)
{
    core = &core_symbol;
    core_symbol.signal_tool_changed().connect(sigc::mem_fun(*this, &ImpBase::handle_tool_change));
}

void ImpSymbol::canvas_update()
{
    canvas->update(*core_symbol.get_canvas_data());
    symbol_preview_window->update(*core_symbol.get_canvas_data());
}

void ImpSymbol::update_monitor()
{
    set_monitor_items({{ObjectType::UNIT, core_symbol.get_symbol()->unit->uuid}});
}

void ImpSymbol::apply_preferences()
{
    symbol_preview_window->set_canvas_appearance(preferences.canvas_non_layer.appearance);
    ImpBase::apply_preferences();
}

void ImpSymbol::construct()
{

    main_window->set_title("Symbol - Interactive Manipulator");
    state_store = std::make_unique<WindowStateStore>(main_window, "imp-symbol");

    symbol_preview_window = new SymbolPreviewWindow(main_window);
    symbol_preview_window->set_text_placements(core.y->get_symbol(false)->text_placements);
    symbol_preview_window->signal_changed().connect([this] { core_symbol.set_needs_save(); });
    core_symbol.signal_tool_changed().connect(
            [this](ToolID tool_id) { symbol_preview_window->set_can_load(tool_id == ToolID::NONE); });
    symbol_preview_window->signal_load().connect([this](int angle, bool mirror) {
        if (core_symbol.tool_is_active())
            return;
        auto pl = symbol_preview_window->get_text_placements();
        for (auto &it : core_symbol.get_symbol()->texts) {
            auto key = std::make_tuple(angle, mirror, it.first);
            if (pl.count(key))
                it.second.placement = pl.at(key);
        }
        core_symbol.commit();
        core_symbol.rebuild();
        auto sel = canvas->get_selection();
        canvas_update();
        canvas->set_selection(sel);
    });

    {
        auto button = Gtk::manage(new Gtk::Button("Preview..."));
        main_window->header->pack_start(*button);
        button->show();
        button->signal_clicked().connect([this] { symbol_preview_window->present(); });
    }

    unplaced_box = Gtk::manage(new UnplacedBox("Pin"));
    unplaced_box->show();
    main_window->left_panel->pack_end(*unplaced_box, true, true, 0);
    unplaced_box->signal_place().connect([this](const auto &items) {
        std::set<SelectableRef> sel;
        for (const auto &it : items) {
            sel.emplace(it, ObjectType::SYMBOL_PIN);
        }
        this->tool_begin(ToolID::MAP_PIN, true, sel);
    });

    core_symbol.signal_rebuilt().connect(sigc::mem_fun(*this, &ImpSymbol::update_unplaced));
    update_unplaced();
    core_symbol.signal_tool_changed().connect(
            [this](ToolID tool_id) { unplaced_box->set_sensitive(tool_id == ToolID::NONE); });

    auto header_button = Gtk::manage(new HeaderButton);
    header_button->set_label(core.y->get_symbol()->name);
    main_window->header->set_custom_title(*header_button);
    header_button->show();

    name_entry = header_button->add_entry("Name");
    name_entry->set_text(core.y->get_symbol()->name);
    name_entry->set_width_chars(std::min<int>(core.y->get_symbol()->name.size(), 20));
    name_entry->signal_changed().connect([this, header_button] {
        header_button->set_label(name_entry->get_text());
        core_symbol.set_needs_save();
    });

    unit_label = Gtk::manage(new Gtk::Label(core.y->get_symbol()->unit->name));
    unit_label->set_xalign(0);
    unit_label->set_selectable(true);
    header_button->add_widget("Unit", unit_label);

    can_expand_switch = Gtk::manage(new Gtk::Switch);
    can_expand_switch->set_active(core.y->get_symbol()->can_expand);
    can_expand_switch->set_halign(Gtk::ALIGN_START);
    can_expand_switch->property_active().signal_changed().connect([this] { core_symbol.set_needs_save(); });

    header_button->add_widget("Can expand", can_expand_switch);

    {
        auto box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
        box->get_style_context()->add_class("linked");
        auto rb_primary = Gtk::manage(new Gtk::RadioButton("Primary"));
        rb_primary->set_mode(false);
        auto rb_alt = Gtk::manage(new Gtk::RadioButton("Alt."));
        rb_alt->set_mode(false);
        auto rb_both = Gtk::manage(new Gtk::RadioButton("Both"));
        rb_both->set_mode(false);
        rb_alt->join_group(*rb_primary);
        rb_both->join_group(*rb_primary);
        box->pack_start(*rb_primary, true, true, 0);
        box->pack_start(*rb_alt, true, true, 0);
        box->pack_start(*rb_both, true, true, 0);
        box->show_all();
        header_button->add_widget("Pin display", box);
        rb_primary->signal_toggled().connect([this, rb_primary] {
            if (rb_primary->get_active()) {
                core_symbol.set_pin_display_mode(Symbol::PinDisplayMode::PRIMARY);
                canvas_update_from_pp();
            }
        });
        rb_alt->signal_toggled().connect([this, rb_alt] {
            if (rb_alt->get_active()) {
                core_symbol.set_pin_display_mode(Symbol::PinDisplayMode::ALT);
                canvas_update_from_pp();
            }
        });
        rb_both->signal_toggled().connect([this, rb_both] {
            if (rb_both->get_active()) {
                core_symbol.set_pin_display_mode(Symbol::PinDisplayMode::BOTH);
                canvas_update_from_pp();
            }
        });
        core_symbol.signal_tool_changed().connect(
                [this, box](ToolID tool_id) { box->set_sensitive(tool_id == ToolID::NONE); });
    }

    core.r->signal_rebuilt().connect([this] { unit_label->set_text(core.y->get_symbol()->unit->name); });

    core.r->signal_save().connect([this, header_button] {
        auto sym = core.y->get_symbol(false);
        sym->name = name_entry->get_text();
        header_button->set_label(sym->name);
        sym->text_placements = symbol_preview_window->get_text_placements();
        sym->can_expand = can_expand_switch->get_active();
    });
    grid_spin_button->set_sensitive(false);
    update_monitor();

    connect_action(ActionID::EDIT_UNIT, [this](const auto &a) {
        this->edit_pool_item(ObjectType::UNIT, core_symbol.get_symbol()->unit->uuid);
    });
    set_action_sensitive(make_action(ActionID::EDIT_UNIT), sockets_connected);

    hamburger_menu->append("Change unit", "win.change_unit");
    add_tool_action(ToolID::CHANGE_UNIT, "change_unit");

    if (sockets_connected) {
        hamburger_menu->append("Edit unit", "win.edit_unit");
        main_window->add_action("edit_unit", [this] { trigger_action(ActionID::EDIT_UNIT); });
    }
}

void ImpSymbol::update_unplaced()
{
    const auto &pins_from_symbol = core_symbol.get_symbol()->pins;
    std::map<UUID, std::string> unplaced;
    for (const auto &it : core_symbol.get_symbol()->unit->pins) {
        if (pins_from_symbol.count(it.first) == 0) {
            unplaced.emplace(it.first, it.second.primary_name);
        }
    }
    unplaced_box->update(unplaced);
}

} // namespace horizon
