#include "selection_filter_dialog.hpp"
#include "util/gtk_util.hpp"
#include "common/common.hpp"
#include "common/object_descr.hpp"
#include "core/core.hpp"
#include "canvas/selection_filter.hpp"

namespace horizon {


SelectionFilterDialog::SelectionFilterDialog(Gtk::Window *parent, SelectionFilter *sf, Core *c)
    : Gtk::Window(), selection_filter(sf), core(c)
{
    set_default_size(220, 300);
    set_type_hint(Gdk::WINDOW_TYPE_HINT_DIALOG);
    set_transient_for(*parent);
    auto hb = Gtk::manage(new Gtk::HeaderBar());
    hb->set_show_close_button(true);
    set_titlebar(*hb);
    hb->show();
    set_title("Selection filter");

    reset_button = Gtk::manage(new Gtk::Button());
    reset_button->set_image_from_icon_name("object-select-symbolic", Gtk::ICON_SIZE_BUTTON);
    reset_button->set_tooltip_text("Select all");
    reset_button->show_all();
    reset_button->signal_clicked().connect([this] {
        for (auto cb : checkbuttons) {
            cb->set_active(true);
        }
        update_reset_button_sensitivity();
    });
    hb->pack_start(*reset_button);
    reset_button->show();

    listbox = Gtk::manage(new Gtk::ListBox());
    listbox->set_selection_mode(Gtk::SELECTION_NONE);
    listbox->set_header_func(sigc::ptr_fun(&header_func_separator));
    for (const auto &it : object_descriptions) {
        auto ot = it.first;
        if (ot == ObjectType::POLYGON)
            continue;
        if (core->has_object_type(ot)) {
            auto cb = Gtk::manage(new Gtk::CheckButton(it.second.name_pl));
            cb->set_active(true);
            cb->property_margin() = 3;
            cb->signal_toggled().connect([this, ot, cb] {
                selection_filter->object_filter[ot] = cb->get_active();
                update_reset_button_sensitivity();
            });
            cb->signal_button_press_event().connect(
                    [this, cb](GdkEventButton *ev) {
                        if (ev->type == GDK_2BUTTON_PRESS) {
                            for (auto cb_other : checkbuttons) {
                                cb_other->set_active(cb == cb_other);
                            }
                        }
                        else if (ev->type == GDK_BUTTON_PRESS) {
                            cb->set_active(!cb->get_active());
                        }
                        update_reset_button_sensitivity();
                        return true;
                    },
                    false);
            checkbuttons.push_back(cb);

            listbox->append(*cb);
        }
    }

    auto sc = Gtk::manage(new Gtk::ScrolledWindow());
    sc->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
    sc->add(*listbox);

    auto box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
    box->pack_start(*sc, true, true, 0);

    {
        auto sep = Gtk::manage(new Gtk::Separator(Gtk::ORIENTATION_HORIZONTAL));
        box->pack_start(*sep, false, false, 0);
    }

    {
        auto la = Gtk::manage(new Gtk::Label("Double click to select just one type."));
        make_label_small(la);
        la->property_margin() = 3;
        la->get_style_context()->add_class("dim-label");
        box->pack_start(*la, false, false, 0);
    }
    add(*box);
    box->show_all();
    update_reset_button_sensitivity();
}

void SelectionFilterDialog::update_reset_button_sensitivity()
{
    reset_button->set_sensitive(
            !std::all_of(checkbuttons.begin(), checkbuttons.end(), [](auto x) { return x->get_active(); }));
}

} // namespace horizon
