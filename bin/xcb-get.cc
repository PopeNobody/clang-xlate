#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <iostream>
#include <string>

Display *display;
Window window;
Atom selection, utf8_string, property, xa_targets;

// Function to handle selection events
void get_selection_data() {
    XConvertSelection(
        display, selection, utf8_string, property, window, CurrentTime);
}

// Event loop and handler
void event_loop() {
    XEvent event;
    while (true) {
        XNextEvent(display, &event);

        if (event.type == SelectionNotify) {
            if (event.xselection.property != None) {
                // Data is ready, retrieve it
                Atom type;
                int format;
                unsigned long nitems, bytes_after;
                unsigned char *data;
                
                XGetWindowProperty(
                    display, window, event.xselection.property, 
                    0, ~0L, True, AnyPropertyType, &type, 
                    &format, &nitems, &bytes_after, &data);
                
                if (type == utf8_string) {
                    std::cout << "Clipboard content (C++): " << std::endl;
                    std::cout << std::string((char*)data, nitems) << std::endl;
                }
                XFree(data);
            } else {
                std::cerr << "Selection request failed (clipboard empty?)." << std::endl;
            }
            break; // Exit loop
        }
    }
}

int main() {
    display = XOpenDisplay(NULL);
    if (!display) {
        std::cerr << "Cannot open display" << std::endl;
        return 1;
    }

    int screen = DefaultScreen(display);
    window = XCreateSimpleWindow(
        display, RootWindow(display, screen), 
        1, 1, 1, 1, 0,
        BlackPixel(display, screen),
        WhitePixel(display, screen));


    // Get Atoms
    selection = XInternAtom(display, "CLIPBOARD", False);
    utf8_string = XInternAtom(display, "UTF8_STRING", False);
    property = XInternAtom(display, "CLIPBOARD_CONTENTS", False);
    xa_targets = XInternAtom(display, "TARGETS", False);


    // Check if the clipboard is owned before requesting (a robust check)
    Window owner = XGetSelectionOwner(display, selection);
    if (owner == None) {
        std::cerr << "Clipboard currently empty." << std::endl;
    } else {
        get_selection_data();
        event_loop();
    }


    XDestroyWindow(display, window);
    XCloseDisplay(display);

    return 0;
}
