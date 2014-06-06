#include "main.h"

static void textout(int x, int y, wchar_t *str, uint16_t length, int d, int h1, int h2)
{
    h1 -= d;
    h2 -= d;

    if(h1 < 0) {
        h1 = 0;
    }

    if(h2 < 0) {
        h2 = 0;
    }

    if(h1 > length) {
        h1 = length;
    }

    if(h2 > length) {
        h2 = length;
    }

    SIZE size;

    TextOutW(hdc, x, y, str, h1);
    GetTextExtentPoint32W(hdc, str, h1, &size);
    x += size.cx;

    setbgcolor(TEXT_HIGHLIGHT_BG);
    setcolor(TEXT_HIGHLIGHT);

    TextOutW(hdc, x, y, str + h1, h2 - h1);
    GetTextExtentPoint32W(hdc, str + h1, h2 - h1, &size);
    x += size.cx;

    setbgcolor(~0);
    setcolor(0);

    TextOutW(hdc, x, y, str + h2, length - h2);
}

static int drawmsg(int x, int y, wchar_t *str, uint16_t length, int h1, int h2)
{
    _Bool word = 0;
    int xc = x;
    wchar_t *a = str, *b = str, *end = str + length;
    while(a != end) {
        switch(*a) {
        case '\n': {
            textout(x, y, b, a - b, b - str, h1, h2);
            y += font_msg_lineheight;
            x = xc;
            b = a + 1;

            setcolor(0);
            word = 0;
            break;
        }

        case ' ': {
            if(word) {
                SIZE size;
                int count = a - b;
                textout(x, y, b, count, b - str, h1, h2);
                GetTextExtentPoint32W(hdc, b, count, &size);
                x += size.cx;
                b = a;

                setcolor(0);
                word = 0;
            }

            if((end - a >= 8 && memcmp(a, L" http://", 16) == 0) || (end - a >= 9 && memcmp(a, L" https://", 18) == 0)) {
                SIZE size;
                int count = a - b;
                textout(x, y, b, count,  b - str, h1, h2);
                GetTextExtentPoint32W(hdc, b, count, &size);
                x += size.cx;
                b = a;

                setcolor(0xFF0000);
                word = 1;
            }
            break;
        }
        }
        a++;
    }

    textout(x, y, b, a - b, b - str, h1, h2);
    y += font_msg_lineheight;
    setcolor(0);

    //RECT r = {x, y, x + width, bottom};
    //y += DrawTextW(hdc, out, length, &r, DT_WORDBREAK | DT_NOPREFIX);

    return y;
}

static uint32_t pmsg(int mx, int my, wchar_t *str, uint16_t length)
{
    wchar_t *a = str, *b = str, *end = str + length;
    while(a != end) {
        if(*a == '\n') {
            if(my >= 0 && my <= font_msg_lineheight) {
                break;
            }
            b = a;
            my -= font_msg_lineheight;
        }
        a++;
    }

    int fit;
    mx -= 110;
    if(mx > 0) {
        int len = a - b, d[len];
        SIZE size;
        GetTextExtentExPointW(hdc, b, len, mx, &fit, d, &size);
    } else {
        fit = 0;
    }

    return (b - str) + fit;
}

static int heightmsg(wchar_t *str, uint16_t length)
{
    int y = 0;
    wchar_t *a = str, *end = str + length;
    while(a != end) {
        if(*a == '\n') {
            y += font_msg_lineheight;
        }
        a++;
    }

    y += font_msg_lineheight;

    return y;
}

void messages_draw(MESSAGES *m, int x, int y, int width, int height)
{
    setcolor(0);
    setfont(FONT_MSG);

    uint8_t lastauthor = 0xFF;

    void **p = m->data->data;
    int i = 0, n = m->data->n;

    while(i != n) {
        MESSAGE *msg = *p++;

        /*uint8_t author = msg->flags & 1;
        if(author != lastauthor) {
            if(!author) {
                setfont(FONT_MSG_NAME);
                drawtextwidth_right(x, 95, y, f->name, f->name_length);
                setfont(FONT_MSG);
            } else {
                setcolor(0x888888);
                drawtextwidth_right(x, 95, y, self.name, self.name_length);
                setcolor(0);
            }
            lastauthor = author;
        }*/

        switch(msg->flags) {
        case 0:
        case 1:
        case 2:
        case 3: {
            /* normal message */
            if(i == m->data->istart) {
                y = drawmsg(x + 110, y, msg->msg, msg->length, m->data->start, ((i == m->data->iend) ? m->data->end : msg->length));
            } else if(i == m->data->iend) {
                y = drawmsg(x + 110, y, msg->msg, msg->length, 0, m->data->end);
            } else if(i > m->data->istart && i < m->data->iend) {
                y = drawmsg(x + 110, y, msg->msg, msg->length, 0, msg->length);
            } else {
                y = drawmsg(x + 110, y, msg->msg, msg->length, 0, 0);
            }

            break;
        }

        case 4:
        case 5: {
            /* image */
            MSG_IMG *img = (void*)msg;
            SIZE size;

            SelectObject(hdcMem, img->bitmap);
            GetBitmapDimensionEx(img->bitmap, &size);
            BitBlt(hdc, x, y, size.cx, size.cy, hdcMem, 0, 0, SRCCOPY);
            y += img->height;
            break;
        }

        case 6:
        case 7: {
            MSG_FILE *file = (void*)msg;
            int dx = 110;

            uint8_t size[16];
            int sizelen = sprint_bytes(size, file->size);

            switch(file->status) {
            case FILE_PENDING: {
                if(msg->flags == 6) {
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, "wants to share file ");
                    setcolor(0);
                    dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, " (");
                    setcolor(0);
                    dx += drawtext_getwidth(x + dx, y, size, sizelen);
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, ") ");
                    setcolor(COLOR_LINK);
                    setfont(FONT_MSG_LINK);
                    dx += drawstr_getwidth(x + dx, y, "Accept");
                    drawstr(x + dx + 10, y, "Decline");
                    setfont(FONT_MSG);
                } else {
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, "offering file ");
                    setcolor(0);
                    dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, " (");
                    setcolor(0);
                    dx += drawtext_getwidth(x + dx, y, size, sizelen);
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, ") ");
                    setcolor(COLOR_LINK);
                    setfont(FONT_MSG_LINK);
                    drawstr(x + dx, y, "Cancel");
                    setfont(FONT_MSG);
                }

                break;
            }

            case FILE_OK: {
                setcolor(0x888888);
                dx += drawstr_getwidth(x + dx, y, "transferring file ");
                setcolor(0);
                dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);

                setcolor(COLOR_LINK);
                setfont(FONT_MSG_LINK);
                dx += drawstr_getwidth(x + dx + 10, y, "Pause");
                drawstr(x + dx + 20, y, "Cancel");
                setfont(FONT_MSG);
                break;
            }

            case FILE_PAUSED: {
                setcolor(0x888888);
                dx += drawstr_getwidth(x + dx, y, "transferring file (paused) ");
                setcolor(0);
                dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);

                setcolor(COLOR_LINK);
                setfont(FONT_MSG_LINK);
                dx += drawstr_getwidth(x + dx + 10, y, "Resume");
                drawstr(x + dx + 20, y, "Cancel");
                setfont(FONT_MSG);
                break;
            }

            case FILE_BROKEN: {
                //"cancelled file winTox.png (150KiB)"

                setcolor(0x888888);
                dx += drawstr_getwidth(x + dx, y, "transferring file (disconnected) ");
                setcolor(0);
                dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);

                setcolor(COLOR_LINK);
                setfont(FONT_MSG_LINK);
                drawstr(x + dx + 10, y, "Cancel");
                setfont(FONT_MSG);
                break;
            }

            case FILE_KILLED: {
                //"cancelled file winTox.png (150KiB)"

                setcolor(0x888888);
                dx += drawstr_getwidth(x + dx, y, "cancelled file ");
                setcolor(0);
                dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);

                break;
            }

            case FILE_DONE: {
                //"sent file winTox.png (150KiB) Open"
                if(msg->flags == 6) {
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, "transferred file ");
                    setcolor(0);
                    dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);

                    setcolor(COLOR_LINK);
                    setfont(FONT_MSG_LINK);
                    drawstr(x + dx + 10, y, "Open");
                    setfont(FONT_MSG);

                } else {
                    setcolor(0x888888);
                    dx += drawstr_getwidth(x + dx, y, "transferred file ");
                    setcolor(0);
                    dx += drawtext_getwidth(x + dx, y, file->name, file->name_length);
                }

                break;
            }
            }

            y += font_msg_lineheight;



            if(file->status != FILE_PENDING && file->status < FILE_KILLED) {
                uint64_t progress = (file->progress > file->size) ? file->size : file->progress;
                uint32_t x1 = (uint64_t)400 * progress / file->size;
                RECT r = {x + 110, y, x + 110 + x1, y + font_msg_lineheight};
                fillrect(&r, BLUE);
                r.left = r.right;
                r.right = x + 110 + 400;
                fillrect(&r, 0x999999);

                SetTextAlign(hdc, TA_CENTER | TA_TOP | TA_NOUPDATECP);

                char text[128];
                int textlen = sprintf(text, "%"PRIu64"/%"PRIu64, file->progress, file->size);
                TextOut(hdc, x + 110 + 200, y, text, textlen);

                SetTextAlign(hdc, TA_LEFT | TA_TOP | TA_NOUPDATECP);

                y += font_msg_lineheight;
            }



            break;
        }
        }

        i++;
    }
}

_Bool messages_mmove(MESSAGES *m, int mx, int my, int dy, int width, int height)
{
    if(my < 0) {
        my = 0;
    }

    if(mx < 0 || mx >= width) {
        m->iover = ~0;
        return 0;
    }

    setfont(FONT_MSG);

    void **p = m->data->data;
    int i = 0, n = m->data->n;

    while(i != n) {
        MESSAGE *msg = *p++;

        int dy = msg->height;

        if((my >= 0 && my < dy) || i == n - 1) {
            m->over = 0;
            switch(msg->flags) {
            case 0:
            case 1: {
                /* normal message */
                m->over = pmsg(mx, my, msg->msg, msg->length);
                break;
            }

            case 2:
            case 3: {
                /* action */
                m->over = pmsg(mx, my, msg->msg, msg->length);
                break;
            }

            case 4:
            case 5: {
                /* image */
                //MSG_IMG *img = (void*)msg;

                //SIZE size;
                //GetBitmapDimensionEx(img->bitmap, &size);
                break;
            }

            case 6:
            case 7: {
                if(my >= font_msg_lineheight) {break;}
                /* file transfer */
                MSG_FILE *file = (void*)msg;
                mx -= 110;

                uint8_t size[16];
                int sizelen = sprint_bytes(size, file->size);

                switch(file->status) {
                case FILE_PENDING: {
                    if(msg->flags == 6) {
                        SIZE sz;
                        char str[64];
                        int x1, x2, strlen;

                        strlen = sprintf(str, "wants to share file %.*s (%.*s) ", file->name_length, file->name, sizelen, size);
                        GetTextExtentPoint32(hdc, str, strlen, &sz);
                        x1 = sz.cx;
                        GetTextExtentPoint32(hdc, "Accept", 6, &sz);
                        x2 = x1 + sz.cx;

                        if(mx >= x1 && mx < x2) {
                            hand = 1;
                            m->over = 1;
                            break;
                        }

                        x1 = x2 + 10;
                        GetTextExtentPoint32(hdc, "Decline", 7, &sz);
                        x2 = x1 + sz.cx;

                        if(mx >= x1 && mx < x2) {
                            hand = 1;
                            m->over = 2;
                            break;
                        }
                    } else {
                        SIZE sz;
                        char str[64];
                        int x1, x2, strlen;

                        strlen = sprintf(str, "offering file %.*s (%.*s) ", file->name_length, file->name, sizelen, size);
                        GetTextExtentPoint32(hdc, str, strlen, &sz);
                        x1 = sz.cx;
                        GetTextExtentPoint32(hdc, "Cancel", 6, &sz);
                        x2 = x1 + sz.cx;

                        if(mx >= x1 && mx < x2) {
                            hand = 1;
                            m->over = 1;
                            break;
                        }
                    }

                    break;
                }

                case FILE_OK: {
                    SIZE sz;
                    char str[64];
                    int x1, x2, strlen;

                    strlen = sprintf(str, "transferring file %.*s", file->name_length, file->name);
                    GetTextExtentPoint32(hdc, str, strlen, &sz);
                    x1 = sz.cx + 10;
                    GetTextExtentPoint32(hdc, "Pause", 5, &sz);
                    x2 = x1 + sz.cx;

                    if(mx >= x1 && mx < x2) {
                        hand = 1;
                        m->over = 1;
                        break;
                    }

                    x1 = x2 + 10;
                    GetTextExtentPoint32(hdc, "Cancel", 6, &sz);
                    x2 = x1 + sz.cx;

                    if(mx >= x1 && mx < x2) {
                        hand = 1;
                        m->over = 2;
                        break;
                    }
                    break;
                }

                case FILE_PAUSED: {
                    SIZE sz;
                    char str[64];
                    int x1, x2, strlen;

                    strlen = sprintf(str, "transferring file (paused) %.*s", file->name_length, file->name);
                    GetTextExtentPoint32(hdc, str, strlen, &sz);
                    x1 = sz.cx + 10;
                    GetTextExtentPoint32(hdc, "Resume", 6, &sz);
                    x2 = x1 + sz.cx;

                    if(mx >= x1 && mx < x2) {
                        hand = 1;
                        m->over = 1;
                        break;
                    }

                    x1 = x2 + 10;
                    GetTextExtentPoint32(hdc, "Cancel", 6, &sz);
                    x2 = x1 + sz.cx;

                    if(mx >= x1 && mx < x2) {
                        hand = 1;
                        m->over = 2;
                        break;
                    }
                    break;
                }

                case FILE_BROKEN: {
                    //"cancelled file winTox.png (150KiB)"

                    SIZE sz;
                    char str[64];
                    int x1, x2, strlen;

                    strlen = sprintf(str, "transferring file %.*s", file->name_length, file->name);
                    GetTextExtentPoint32(hdc, str, strlen, &sz);
                    x1 = sz.cx + 10;
                    GetTextExtentPoint32(hdc, "Cancel", 6, &sz);
                    x2 = x1 + sz.cx;

                    if(mx >= x1 && mx < x2) {
                        hand = 1;
                        m->over = 1;
                        break;
                    }

                    break;
                }

                case FILE_KILLED: {
                    break;
                }

                case FILE_DONE: {
                    //"sent file winTox.png (150KiB) Open"
                    if(msg->flags == 6) {
                        SIZE sz;
                        char str[64];
                        int x1, x2, strlen;

                        strlen = sprintf(str, "transferred file %.*s", file->name_length, file->name);
                        GetTextExtentPoint32(hdc, str, strlen, &sz);
                        x1 = sz.cx + 10;
                        GetTextExtentPoint32(hdc, "Open", 4, &sz);
                        x2 = x1 + sz.cx;

                        if(mx >= x1 && mx < x2) {
                            hand = 1;
                            m->over = 1;
                            break;
                        }


                    }

                    break;
                }
                }
                break;
            }
            }

            m->iover = i;

            if(m->select) {
                if(i > m->idown) {
                    m->data->istart = m->idown;
                    m->data->iend = i;

                    m->data->start = m->down;
                    m->data->end = m->over;
                } else if(i < m->idown) {
                    m->data->iend = m->idown;
                    m->data->istart = i;

                    m->data->end = m->down;
                    m->data->start = m->over;
                } else {
                    m->data->istart = m->data->iend = i;
                    if(m->over >= m->down) {
                        m->data->start = m->down;
                        m->data->end = m->over;
                    } else {
                        m->data->end = m->down;
                        m->data->start = m->over;
                    }
                }

                //debug("test: %u %u %u %u\n", f->istart, f->start, f->iend, f->end);
                return 1;
            }
            return 0;
        }

        my -= dy;

        i++;
    }

    return 0;
}

_Bool messages_mdown(MESSAGES *m)
{
    if(m->iover != ~0) {
        MESSAGE *msg = m->data->data[m->iover];
        switch(msg->flags)
        {
            case 0 ... 3:
            {
                m->data->istart = m->data->iend = m->idown = m->iover;
                m->data->start = m->data->end = m->down = m->over;
                m->select = 1;
                break;
            }

            case 6 ... 7:
            {
                MSG_FILE *file = (void*)msg;
                if(m->over == 0)
                {
                    break;
                }

                switch(file->status) {
                    case FILE_PENDING:
                    {
                        if(msg->flags == 6)
                        {
                            if(m->over == 1) {
                                char *path = malloc(256);
                                memcpy(path, file->name, file->name_length);
                                path[file->name_length] = 0;

                                OPENFILENAME ofn = {
                                    .lStructSize = sizeof(OPENFILENAME),
                                    .hwndOwner = hwnd,
                                    .lpstrFile = path,
                                    .nMaxFile = 256,
                                    .Flags = OFN_EXPLORER | OFN_NOCHANGEDIR,
                                };

                                if(GetSaveFileName(&ofn)) {
                                    tox_postmessage(TOX_ACCEPTFILE, m->data->id, file->filenumber, path);
                                } else {
                                    debug("GetSaveFileName() failed\n");
                                }
                            } else {
                                //decline
                                tox_postmessage(TOX_FILE_IN_CANCEL, m->data->id, file->filenumber, NULL);
                            }
                        } else
                        {
                            //cancel
                            tox_postmessage(TOX_FILE_OUT_CANCEL, m->data->id, file->filenumber, NULL);
                        }


                        break;
                    }

                    case FILE_OK:
                    {
                        if(m->over == 1) {
                            //pause
                            tox_postmessage(TOX_FILE_IN_PAUSE + (msg->flags & 1), m->data->id, file->filenumber, NULL);
                        } else {
                            //cancel
                            tox_postmessage(TOX_FILE_IN_CANCEL + (msg->flags & 1), m->data->id, file->filenumber, NULL);
                        }
                        break;
                    }

                    case FILE_PAUSED:
                    {
                        if(m->over == 1) {
                            //resume
                            tox_postmessage(TOX_FILE_IN_RESUME + (msg->flags & 1), m->data->id, file->filenumber, NULL);
                        } else {
                            //cancel
                            tox_postmessage(TOX_FILE_IN_CANCEL + (msg->flags & 1), m->data->id, file->filenumber, NULL);
                        }
                        break;
                    }

                    case FILE_BROKEN:
                    {
                        //cancel
                        tox_postmessage(TOX_FILE_IN_CANCEL + (msg->flags & 1), m->data->id, file->filenumber, NULL);
                        break;
                    }

                    case FILE_DONE:
                    {
                        if(msg->flags == 6) {
                            //open the file
                        }
                        break;
                    }
                }
                break;
            }
        }

    }
    return 0;
}

_Bool messages_mright(MESSAGES *m)
{
    return 0;
}

_Bool messages_mwheel(MESSAGES *m, int height, double d)
{
    return 0;
}

_Bool messages_mup(MESSAGES *m)
{
    m->select = 0;
    return 0;
}

_Bool messages_mleave(MESSAGES *m)
{
    return 0;
}

static int msgheight(MESSAGE *msg)
{
    switch(msg->flags) {
    case 0:
    case 1:
    case 2:
    case 3: {
        return heightmsg(msg->msg, msg->length);
    }

    case 6:
    case 7: {
        MSG_FILE *file = (void*)msg;
        return (file->status != FILE_PENDING && file->status < FILE_KILLED) ? font_msg_lineheight * 2 : font_msg_lineheight;
    }

    }

    return 0;
}

void message_setheight(MESSAGES *m, MESSAGE *msg, MSG_DATA *p)
{
    msg->height = msgheight(msg);
    p->height += msg->height;
    if(m->data == p) {
        m->panel.content_scroll->content_height = p->height;
    }
}

void message_updateheight(MESSAGES *m, MESSAGE *msg, MSG_DATA *p)
{
    int newheight = msgheight(msg);
    p->height += newheight - msg->height;
    msg->height = newheight;
    if(m->data == p) {
        m->panel.content_scroll->content_height = p->height;
    }
}

void message_add(MESSAGES *m, MESSAGE *msg, MSG_DATA *p)
{
    p->data = realloc(p->data, (p->n + 1) * sizeof(void*));
    p->data[p->n++] = msg;

    message_setheight(m, msg, p);
}
