// runtime error handler
int xerror(Display* dpy, XErrorEvent* ee) {
  /* There's no way to check accesses to destroyed windows, thus those cases are
  * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
  * default error handler, which may call exit. */
  if (ee->error_code == BadWindow
      || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
      || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
      || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
      || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
      || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
      || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
      ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

// dummy error handler (for cleanup)
int xerrordummy(Display* dpy, XErrorEvent* ee) {
  return 0;
}

// startup error handler to check if another window manager is already running
int xerrorstart(Display* dpy, XErrorEvent* ee) {
  die("dwm: another window manager is already running");
  return -1;
}
