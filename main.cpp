#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <string>
#include <vector>

using namespace std;

struct WebSite {
    string name;
    string url;
};

struct AppState {
    GtkWidget *stack;
    GtkWidget *home_menu;
    GtkWidget *web_view;
};

const vector<WebSite> SITES = {
    {"IMDb Stream", "https://streamimdb.ru"},
    {"Anikai", "https://anikai.to/"},
    {"Weflix", "https://weflix.to/"},
    {"Youtube", "https://www.youtube.com/"}
};

static void on_site_button_clicked(GtkWidget *button, gpointer user_data) {
    AppState *state = static_cast<AppState*>(user_data);
    const char *url = static_cast<const char*>(g_object_get_data(G_OBJECT(button), "url"));

    if (url) {
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(state->web_view), url);
        gtk_stack_set_visible_child(GTK_STACK(state->stack), state->web_view);
    }
}

static gboolean on_window_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data) {
    AppState *app_state = static_cast<AppState*>(user_data);

    // Get the actual event to extract clean modifiers
    GdkEvent *event = gtk_event_controller_get_current_event(GTK_EVENT_CONTROLLER(controller));
    GdkModifierType clean_state = gdk_event_get_modifier_state(event);

    // Filter out everything except Control
    if ((clean_state & GDK_CONTROL_MASK) && keyval == GDK_KEY_Left) {
        gtk_stack_set_visible_child(GTK_STACK(app_state->stack), app_state->home_menu);
        return TRUE;
    }
    return FALSE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppState *state = g_new0(AppState, 1);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Streaming Launcher");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    state->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(state->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_window_set_child(GTK_WINDOW(window), state->stack);

    // --- HOME MENU ---
    state->home_menu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_halign(state->home_menu, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(state->home_menu, GTK_ALIGN_CENTER);

    GtkWidget *title_label = gtk_label_new("Select a Streaming Platform");
    gtk_box_append(GTK_BOX(state->home_menu), title_label);

    for (const auto &site : SITES) {
        GtkWidget *btn = gtk_button_new_with_label(site.name.c_str());
        g_object_set_data(G_OBJECT(btn), "url", (gpointer)site.url.c_str());
        g_signal_connect(btn, "clicked", G_CALLBACK(on_site_button_clicked), state);
        gtk_box_append(GTK_BOX(state->home_menu), btn);
    }

    gtk_stack_add_child(GTK_STACK(state->stack), state->home_menu);

    // --- WEB VIEW ---
    state->web_view = webkit_web_view_new();
    WebKitSettings *settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(state->web_view));
    webkit_settings_set_user_agent(settings, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36");

    gtk_stack_add_child(GTK_STACK(state->stack), state->web_view);

    // --- KEY CAPTURE CONTROLLER ---
    GtkEventController *key_controller = gtk_event_controller_key_new();
    // Intercept event before child widgets (like WebKit) swallow it
    gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_window_key_pressed), state);
    gtk_widget_add_controller(window, key_controller);

    g_object_set_data_full(G_OBJECT(window), "app_state", state, g_free);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.stream.launcher", G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}