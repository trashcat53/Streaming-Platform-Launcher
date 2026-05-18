#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <string>

static void activate(GtkApplication *app, gpointer user_data) {

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Streaming Launcher");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 800);

    WebKitWebView *webView = WEBKIT_WEB_VIEW(webkit_web_view_new());

    // OPTIONAL: user agent spoofing (helps some sites)
    WebKitSettings *settings = webkit_web_view_get_settings(webView);
    webkit_settings_set_user_agent(
        settings,
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome Safari"
    );

    gtk_window_set_child(GTK_WINDOW(window), GTK_WIDGET(webView));

    // Load streaming site directly
    webkit_web_view_load_uri(
        webView,
        "https://streamimdb.ru"
    );

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(
        "com.stream.launcher",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}