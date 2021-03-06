#include "rf.h"

int main() {
  int price;
  long semstart;
  WINDOW *main_win;
  sqlite3 *db;
  PANEL *panels[4];
  char _[20], file_name[80];

  read_conf("rf.conf", &price, _, _, file_name);

  // Set up ncurses
  setlocale(LC_ALL, "");
  initscr();
  raw();
  keypad(stdscr, true);
  curs_set(0);
  noecho();
  clear();
  main_win = newwin(LINES - 2, COLS, 2, 0);
  keypad(main_win, true);
  refresh();
  box(main_win, 0, 0);
  ESCDELAY = 0;
  wrefresh(main_win);

  /* start_color();
     assume_default_colors(3, 0); */

  const char *RF = "R E A L I S T F O R E N I N G E N";
  attron(A_BOLD);
  mvprintw(0, (COLS - strlen(RF)) >> 1, RF);
  mvprintw(1, 2, "Menu");
  attroff(A_BOLD);
  refresh();

  // Define start of semester
  time_t ts_now = time(NULL);
  struct tm *now = gmtime(&ts_now);
  now->tm_hour = 0;
  now->tm_min = 0;
  now->tm_sec = 0;
  now->tm_mday = 1;
  now->tm_mon = now->tm_mon < 6 ? 0 : 6;
  semstart = mktime(now);

  // Open database
  sqlite3_open(file_name, &db);
  char *errmsg;
  sqlite3_enable_load_extension(db, 1);
  sqlite3_load_extension(db, "./build/libSqliteIcu.so", "sqlite3_icu_init", &errmsg);
  sqlite3_exec(db, "SELECT icu_load_collation('nb_NO', 'NORWEGIAN')",
               NULL, NULL, &errmsg);
  
  char *sql = "CREATE TABLE IF NOT EXISTS members\
 (first_name NCHAR(50) NOT NULL, last_name NCHAR(50) NOT NULL,\
 lifetime TINYINT, timestamp BIGINT NOT NULL, paid INT)";
  sqlite3_exec(db, sql, NULL, NULL, &errmsg);
  stats(db, main_win, IN_BACKGROUND, semstart, price, WAIT);

  // Start main loop
  home(main_win, WAIT);
  menu(db, main_win, panels, semstart, price);

  sqlite3_close(db);
  delwin(main_win);
  clear();
  refresh();
  endwin();
  return 0;
}

void menu(sqlite3 *db, WINDOW *main_win,
          PANEL **panels, long semstart, int price) {
  int active_y = 0, prev_y = 0, ch;
  char **menu_s;
  WINDOW *menu_win, *padw;
  const int MENU_LEN = 5;
  int curr_scroll = 0, x, y;

  panels[0] = new_panel(main_win);

  menu_s = malloc(sizeof(char*) * MENU_LEN);
  menu_s[0] = " Home     ";
  menu_s[1] = " Members  ";
  menu_s[2] = " Stats    ";
  menu_s[3] = " Backup   ";
  menu_s[4] = " Exit     ";

  // Windows for roll-down menus
  menu_win = newwin(MENU_LEN + 2, 12, 2, 0);
  panels[1] = new_panel(menu_win);

  padw = newpad(5000, COLS - 2);
  panels[2] = new_panel(padw);

  getmaxyx(main_win, y, x);
  keypad(menu_win, true);
  box(menu_win, 0, 0);

  for (;;) {
    getmaxyx(main_win, y, x);
    show_panel(panels[1]);
    printmenu(menu_win, menu_s, MENU_LEN, active_y);
    update_panels();
    doupdate();
    
    switch (ch = getch()) {
    case 27:
      if (dialog_sure(main_win, panels)) {
        free(menu_s);
        delwin(menu_win);
        delwin(padw);
        del_panel(panels[0]);
        del_panel(panels[1]);
        del_panel(panels[2]);
        return;
      }
      refresh_bg(db, main_win, padw, prev_y, curr_scroll, semstart, price);
      break;
    case KEY_UP:
      active_y == 0 ? active_y = MENU_LEN - 1 : active_y--;
      break;
    case KEY_DOWN:
      active_y == MENU_LEN - 1 ? active_y = 0 : active_y++;
      break;
    case 10:
      switch (active_y) {
      case 0:
        home(main_win, WAIT);
        break;
      case 1:
        members(db, main_win, padw, panels, IN_FOREGROUND,
                semstart, (long) time(NULL), &curr_scroll, price);
        break;
      case 2:
        stats(db, main_win, IN_FOREGROUND, semstart, price, WAIT);
        break;
      case 3:
        backup(main_win, panels);
        refresh_bg(db, main_win, padw, prev_y, curr_scroll, semstart, price);
        break;
      case 4:
        if (dialog_sure(main_win, panels)) {
          free(menu_s);
          delwin(menu_win);
          delwin(padw);
          del_panel(panels[0]);
          del_panel(panels[1]);
          del_panel(panels[2]);
          return;
        }
        refresh_bg(db, main_win, padw, prev_y, curr_scroll, semstart, price);
      }
      prev_y = active_y != 4
        && active_y != 5 ? active_y : prev_y;
    }
  }
}

void refresh_bg(sqlite3 *db, WINDOW *main_win, WINDOW *padw, int prev_y,
                int curr_scroll, long semstart, int price) {
  int x, y;
  getmaxyx(main_win, y, x);
  switch (prev_y) {
  case 0:
    home(main_win, NOWAIT);
    break;
  case 1:
    prefresh(padw, curr_scroll, 1, 3, 1, y, x-2);
    break;
  case 2:
    stats(db, main_win, IN_FOREGROUND, semstart, price, NOWAIT);
    break;
  }
}

void printmenu(WINDOW *mw, char **menu_s, int ML, int active) {
  int y = 1, x = 1, i;
  box(mw, 0, 0);
  for (i = 0; i < ML; i++) {
    i == active ? wattron(mw, A_REVERSE) : 0;
    mvwprintw(mw, y++, x, menu_s[i]);
    i == active ? wattroff(mw, A_REVERSE) : 0;
  }
  wrefresh(mw);
}

void home(WINDOW *main_win, int w) {
  int i;
  char *bear[27];
  int x, y, ch;

  getmaxyx(main_win, y, x);
  werase(main_win);
  box(main_win, 0, 0);

  bear[0] = "                         _____";
  bear[1] = "                      _d       b_";
  bear[2] = "                   _d             b_";
  bear[3] = "                 _d                 b_";
  bear[4] = "              _d                      b_";
  bear[5] = "         (\\____ /\\                      b";
  bear[6] = "         /      a )                      b";
  bear[7] = "        ( Ø  Ø                            b";
  bear[8] = "     d888b       _  \\                       b";
  bear[9] = "     qO8Op      ´|` |                        b";
  bear[10] = "      (_____,--´´  /                         b";
  bear[11] = "          (___,---´                           8";
  bear[12] = " ________    \\88\\/88                           8";
  bear[13] = "(____  / \\..__88/\\88          |         |      8";
  bear[14] = "   (__(_      |          _____|__       |      8";
  bear[15] = "    (___     /        ,/´        `\\     |      8";
  bear[16] = "     (__)____|       /                  /      8";
  bear[17] = "             \\      ///                /       8";
  bear[18] = "             |      ||| | ___         /        p";
  bear[19] = "             |      `\\\\_\\/   `\\_____,/        |";
  bear[20] = "             |                               |";
  bear[21] = "             |                               |";
  bear[22] = "             \\            __              /\\ (";
  bear[23] = "             /           / /            ./  \\\\";
  bear[24] = "            |           / (            (";
  bear[25] = "          __|,.,.       |_|,.,.        \\";
  bear[26] = "         ((_(_(_,__,___,((_(_(_,__,_____)";

  for (i = 0; i < 27; i++)
    mvwprintw(main_win, (y >> 1) - 15 + i,
              ((x - strlen(bear[12])) >> 1) - 3, bear[i]);

  char *s = "Dette er Realistforeningens medlemsliste.";
  mvwprintw(main_win, (y >> 1) - 14 + i, (x - strlen(s)) >> 1, s);
  wrefresh(main_win);
  if (w == NOWAIT)
    return;
  for (;;) {
    switch (ch = getch()) {
    case 27:
      return;
    }
  }
}

void reg(sqlite3 *db, WINDOW *main_win, WINDOW *padw, PANEL **panels,
         int curr_scroll, int curr_line, int visible_members,
         int delete_rowid, long semstart, int price) {
  int x, y, ch, h = 15, wi = 50, i;
  FIELD *fields[3];
  FORM *rf_form;
  WINDOW *formw, *dwin;
  char *f_name, *l_name;
  bool lt = false;
  hide_panel(panels[1]);
  getmaxyx(main_win, y, x);

  for (i = 0; i < 2; i++) {
    fields[i] = new_field(1, 25, 1 + i * 2, 12, 0, 0);
    set_field_back(fields[i], A_UNDERLINE);
  }
  fields[2] = NULL;

  rf_form = new_form(fields);
  scale_form(rf_form, &h, &wi);
  formw = newwin(h + 3, wi + 4, (y - h) >> 1, (x - wi) >> 1);
  panels[3] = new_panel(formw);
  update_panels();
  doupdate();
  keypad(formw, true);
  set_form_win(rf_form, formw);
  dwin = derwin(formw, h, wi + 2, 1, 1);
  keypad(dwin, true);
  set_form_sub(rf_form, dwin);
  box(formw, 0, 0);
  post_form(rf_form);
  mvwprintw(dwin, 1, 1, "  Fornavn:");
  mvwprintw(dwin, 3, 1, "Etternavn:");
  wmove(dwin, 1, 12);
  wrefresh(formw);
  curs_set(1);
  for (;;) {
    switch (ch = wgetch(dwin)) {
    case 12:
      mvwprintw(dwin, 0, (wi>>1) - 3, (lt = !lt) ? "LIVSTID" : "       ");
      wmove(dwin, 1, 12);
      break;
    case KEY_UP:
    case KEY_BTAB:
      form_driver(rf_form, REQ_PREV_FIELD);
      form_driver(rf_form, REQ_END_LINE);
      break;
    case KEY_DOWN:
    case 9:
      form_driver(rf_form, REQ_NEXT_FIELD);
      form_driver(rf_form, REQ_END_LINE);
      break;
    case KEY_LEFT:
      form_driver(rf_form, REQ_PREV_CHAR);
      break;
    case KEY_RIGHT:
      form_driver(rf_form, REQ_NEXT_CHAR);
      break;
    case KEY_BACKSPACE:
      form_driver(rf_form, REQ_DEL_PREV);
      break;
    case 10:
      form_driver(rf_form, REQ_NEXT_FIELD);
      f_name = strstrip(field_buffer(fields[0], 0));
      l_name = strstrip(field_buffer(fields[1], 0));
      if (!(*f_name & *l_name))
        break; // Don't allow empty names
      register_member(db, main_win, f_name, l_name, lt, lt ? 0L : time(NULL),
                      semstart, price);
      search(db, main_win, padw, "", semstart, (long) time(NULL),
             &curr_line, &visible_members, &delete_rowid, curr_scroll);
      prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
      box(formw, 0, 0);
      form_driver(rf_form, REQ_CLR_FIELD);
      form_driver(rf_form, REQ_NEXT_FIELD);
      form_driver(rf_form, REQ_CLR_FIELD);
      form_driver(rf_form, REQ_NEXT_FIELD);
      mvwprintw(dwin, 1, 1, "  Fornavn:");
      mvwprintw(dwin, 3, 1, "Etternavn:");
      wmove(dwin, 1, 12);
      wrefresh(formw);
      break;
    case 27:
      hide_panel(panels[3]);
      update_panels();
      doupdate();
      unpost_form(rf_form);
      free_form(rf_form);
      curs_set(0);
      for (i = 0; i < 2; i++)
        free_field(fields[i]);
      prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
      delwin(formw);
      del_panel(panels[3]);
      return;
    default:
      form_driver(rf_form, ch);
      break;
    }
    wrefresh(formw);
  }
}

void members(sqlite3 *db, WINDOW *main_win, WINDOW *padw, PANEL **panels,
             int bg, long period_begin, long period_end, int *curr_scroll,
             int price) {
  int x, y, ch;
  hide_panel(panels[1]);
  getmaxyx(main_win, y, x);
  werase(main_win);
  box(main_win, 0, 0);
  update_panels();
  doupdate();
  int needle_idx = 0, curr_line = 0, visible_members = 0;
  int delete_rowid = 0;

  *curr_scroll = 0;
  search(db, main_win, padw, "", period_begin, period_end, &curr_line,
         &visible_members, &delete_rowid, *curr_scroll);
  bool search_mode = false;
  char *send_s, *needle_buf = malloc(sizeof(char) * 100);
  needle_buf[0] = '\0';

  bool btm = false; // Hack to make scrolling work
  prefresh(padw, *curr_scroll, 1, 3, 1, y, x-2);
  if (bg)
    return;
  for (;;) {
    switch (ch = getch()) {
    case KEY_BACKSPACE:
      if (search_mode && needle_idx > 0) {
        needle_buf[--needle_idx] = '\0';
        send_s = (char *) malloc(needle_idx + 1);
        strncpy(send_s, needle_buf, needle_idx+1);
        curr_line = 0;
        search(db, main_win, padw, send_s, period_begin, (long) time(NULL),
               &curr_line, &visible_members, &delete_rowid, *curr_scroll);
        free(send_s);
        mvprintw(1, 26, "                         ");
        mvprintw(1, 27, "%s", needle_buf);
      }
      break;
    case KEY_DOWN:
      if (curr_line == visible_members - 1) {
        btm = false;
        break;}
      curr_line++;
      search(db, main_win, padw, needle_buf, period_begin, (long) time(NULL),
             &curr_line, &visible_members, &delete_rowid, *curr_scroll);
      if (curr_line == visible_members - 1 && btm)
        break;
      if (curr_line > y - 3)
        prefresh(padw, ++(*curr_scroll), 1, 3, 1, y, x-2);
      move(1, 27 + needle_idx);
      break;
    case KEY_UP:
      if (curr_line == 0) {
        btm = false;
        break;
      }
      if (curr_line == visible_members - 1)
        btm = true;
      curr_line--;
      search(db, main_win, padw, needle_buf, period_begin, (long) time(NULL),
             &curr_line, &visible_members, &delete_rowid, *curr_scroll);
      if (curr_line < *curr_scroll)
        prefresh(padw, --(*curr_scroll), 1, 3, 1, y, x-2);
      move(1, 27 + needle_idx);
      break;
    case KEY_DC: // Delete
      if (!search_mode) {
        if (!delete(db, main_win, panels, delete_rowid)) {
          prefresh(padw, *curr_scroll, 1, 3, 1, y, x-2);
          break;
        }
        stats(db, main_win, IN_BACKGROUND, period_begin, price, WAIT);
        curr_line = 0;
        search(db, main_win, padw, needle_buf, period_begin, (long) time(NULL),
               &curr_line, &visible_members, &delete_rowid, *curr_scroll);
      }
      break;
    case 47:
      if (!search_mode) {
        curr_line = 0;
        *curr_scroll = 0;
        search_mode = true;
        mvprintw(1, 19, "Search:");
        curs_set(1);
        move(1, 27);
      }
      break;
    case 27:
      if (search_mode) {
        search_mode = false;
        needle_idx = 0;
        curs_set(0);
        werase(padw);
        search(db, main_win, padw, "", period_begin, (long) time(NULL),
               &curr_line, &visible_members, &delete_rowid, *curr_scroll);
        mvprintw(1, 19, "                                ");
        prefresh(padw, *curr_scroll, 1, 3, 1, y, x - 2);
      } else {
        free(needle_buf);
        curr_line = 0;
        prefresh(padw, *curr_scroll, 1, 3, 1, y, x - 2);
        return;
      }
      break;
    case 110: // N for New member
      if (!search_mode) {
        reg(db, main_win, padw, panels, *curr_scroll, curr_line,
            visible_members, delete_rowid, period_begin, price);
        search(db, main_win, padw, "", period_begin, period_end, &curr_line,
               &visible_members, &delete_rowid, *curr_scroll);
        break;
      } // else fall through to default
    case 104: // H for Help
      if (!search_mode) {
        member_help(main_win, panels);
        prefresh(padw, *curr_scroll, 1, 3, 1, y, x-2);
        break;
      } // else fall through to default
    default:
      if (search_mode && needle_idx < 32) {
        werase(padw);
        needle_buf[needle_idx++] = (char) ch;
        needle_buf[needle_idx] = '\0';
        send_s = (char *) malloc(needle_idx + 1);
        strncpy(send_s, needle_buf, needle_idx + 1);
        search(db, main_win, padw, send_s, period_begin, (long) time(NULL),
               &curr_line, &visible_members, &delete_rowid, *curr_scroll);
        free(send_s);
        prefresh(padw, *curr_scroll, 1, 3, 1, y, x - 2);
        mvprintw(1, 26, " %s", needle_buf);
      }
    }
  }
}
  
void member_help(WINDOW *main_win, PANEL **panels) {
  int x, y, ch, h = 7, wi = 23, i = 1;
  WINDOW *dialogw;
  getmaxyx(main_win, y, x);

  dialogw = newwin(h + 3, wi + 4, (y - h) >> 1, (x - wi) >> 1);
  panels[3] = new_panel(dialogw);
  update_panels();
  doupdate();
  box(dialogw, 0, 0);

  wattron(dialogw, A_BOLD);
  mvwprintw(dialogw, i++, 2,   " HELP FOR KEY BINDINGS");
  wattroff(dialogw, A_BOLD);
  i++;
  mvwprintw(dialogw, i++, 2,   "H   - Opens this window");
  mvwprintw(dialogw, i++, 2,   "N   - Add new member");
  mvwprintw(dialogw, i++, 2,   "/   - Search by name");
  mvwprintw(dialogw, i++, 2,   "ESC - Close window or");
  mvwprintw(dialogw, i++, 2,   "      open main menu");


  wrefresh(dialogw);
  for (;;) {
    switch (ch = getch()) {
    case 27: // n
      hide_panel(panels[3]);
      update_panels();
      doupdate();
      delwin(dialogw);
      del_panel(panels[3]);
      return;
    }
  }
}

static char *base36(long value) {
  char base36[37] = "0123456789ABCDEFGHiJKLMNoPQRSTUVWXYZ";
  /* log(2**64) / log(36) = 12.38 => max 13 char + '\0' */
  char buffer[14];
  unsigned int offset = sizeof(buffer);

  buffer[--offset] = '\0';
  do {
    buffer[--offset] = base36[value % 36];
  } while (value /= 36);

  return strdup(&buffer[offset]);
}

void print_y_n(WINDOW *w, int active) {
  int y = 2, x = 3, i;
  char *txt[] = {"    No     ", "    Yes    "};
  for (i = 0; i < 2; i++) {
    i == active ? wattron(w, A_REVERSE) : 0;
    mvwprintw(w, y++, x, txt[i]);
    i == active ? wattroff(w, A_REVERSE) : 0;
  }
  wrefresh(w);
}

bool dialog_sure(WINDOW *main_win, PANEL **panels) {
  int x, y, ch, h = 2, wi = 13, active = 0;
  WINDOW *dialogw;
  getmaxyx(main_win, y, x);  

  dialogw = newwin(h + 3, wi + 4, (y - h) >> 1, (x - wi) >> 1);
  panels[3] = new_panel(dialogw);
  update_panels();
  doupdate();
  box(dialogw, 0, 0);

  mvwprintw(dialogw, 1, 2, "Are you sure?");
  wrefresh(dialogw);
  print_y_n(dialogw, active);
  for (;;) {
    switch (ch = getch()) {
    case KEY_UP:
    case KEY_DOWN:
      print_y_n(dialogw, active = !active);
      break;
    case 121: // y
    case 10:
      if (active || ch == 121) {
      hide_panel(panels[3]);
      update_panels();
      doupdate();
      delwin(dialogw);
      del_panel(panels[3]);
      return true;
      }
    case 110: // n
      hide_panel(panels[3]);
      update_panels();
      doupdate();
      delwin(dialogw);
      del_panel(panels[3]);
      return false;
    }
  }
}

bool delete(sqlite3 *db, WINDOW *main_win, PANEL **panels, int dl) {
  if (!dialog_sure(main_win, panels)) {
    return false;
  }
  char sqls[50];
  sprintf(sqls, "DELETE FROM members WHERE rowid == %d AND\
 lifetime == 0", dl);
  sqlite3_exec(db, sqls, 0, 0, 0);
  return true;
}

void backup(WINDOW *main_win, PANEL **panels) {
  int x, y, h = 15, wi = 70;
  WINDOW *backupw;
  hide_panel(panels[1]);
  getmaxyx(main_win, y, x);
  werase(main_win);
  box(main_win, 0, 0);

  backupw = newwin(h + 3, wi + 4, (y - h) >> 1, (x - wi) >> 1);
  panels[3] = new_panel(backupw);
  update_panels();
  doupdate();
  box(backupw, 0, 0);

  ssh_backup(backupw);
  getch(); // Wait for any key

  hide_panel(panels[3]);
  update_panels();
  doupdate();
  delwin(backupw);
  return;
}

char *get_string(WINDOW *w, int y, int x, bool h) {
  char *pass, *rf, *ch_s;
  int ch, i = 0;
  pass = malloc(sizeof(char)*30);
  ch_s = malloc(sizeof(char)*2);
  curs_set(1);
  wmove(w, y, x);
  for (;;) {
    switch (ch = wgetch(w)) {
    case KEY_BACKSPACE:
      if (i) i--;
      mvwprintw(w, y, x + i, " ");
      wmove(w, y, x + i);
      wrefresh(w);
      break;
    case 10:
      curs_set(0);
      pass[i] = '\0';
      free(ch_s);
      return pass;
    default:
      if (i > 30)
        break;
      pass[i] = ch;
      rf = i % 2 ? "F" : "R";
      sprintf(ch_s, "%c", ch);
      mvwprintw(w, y, x + i++, h ? rf : ch_s);
      wrefresh(w);
      break;
    }
  }
}

int ssh_backup(WINDOW *backupw) {
  ssh_session sshs = ssh_new();
  ssh_scp scp;
  int rc, prev_tmp, line = 1, _;
  long size;
  char *user, domain[80], *password, path[100], *buffer, tmp[300];
  char file_name[80];
  FILE *member_file;

  read_conf("rf.conf", &_, domain, path, file_name);

  mvwprintw(backupw, line++, 2, "Starting backup ...");
  sprintf(tmp, "Enter username for %s:", domain);
  mvwprintw(backupw, line++, 2, tmp);
  wrefresh(backupw);

  if (sshs == NULL)
    return -1;

  user = get_string(backupw, line++, 2, 0);

  sprintf(tmp, "Connecting to %s@%s ...", user, domain);
  mvwprintw(backupw, line, 2, tmp);
  wrefresh(backupw);
  ssh_options_set(sshs, SSH_OPTIONS_HOST, domain);
  ssh_options_set(sshs, SSH_OPTIONS_USER, user);

  prev_tmp = strlen(tmp);

  if ((rc = ssh_connect(sshs)) != SSH_OK) {
    ssh_free(sshs);
    sprintf(tmp, " Could not connect.");
    mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
    sprintf(tmp, "Check internet connection or something.");
    mvwprintw(backupw, line, 2, tmp);
    wrefresh(backupw);
    return -1;
  }

  sprintf(tmp, " Connected!");
  mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
  wrefresh(backupw);

  // TODO Authenticate server
  sprintf(tmp, "Enter password: ");
  mvwprintw(backupw, line, 2, tmp);
  wrefresh(backupw);
  password = get_string(backupw, line, 18, 1);

  sprintf(tmp, "Authenticating ...");
  mvwprintw(backupw, ++line, 2, tmp);
  wrefresh(backupw);

  prev_tmp = strlen(tmp);
  if ((rc = ssh_userauth_password(sshs, NULL, password))
      != SSH_AUTH_SUCCESS) {
    ssh_disconnect(sshs);
    ssh_free(sshs);
    sprintf(tmp, " Failed.");
    mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
    sprintf(tmp, "Wrong password for user '%s'.", user);
    mvwprintw(backupw, line, 2, tmp);
    wrefresh(backupw);
    return -1;
  }
  sprintf(tmp, " OK");
  mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
  wrefresh(backupw);

  sprintf(tmp, "Opening SCP connection to %s ...", path);
  mvwprintw(backupw, line, 2, tmp);
  wrefresh(backupw);
  if ((scp = ssh_scp_new(sshs, SSH_SCP_WRITE, path)) == NULL) {
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }
  if ((rc = ssh_scp_init(scp)) != SSH_OK) {
    // TODO Say so!
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }
  prev_tmp = strlen(tmp);
  sprintf(tmp, " Done!");
  mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
  wrefresh(backupw);

  // Assuming this file will never be bigger than 8 MiB
  buffer = malloc(0x800000);

  sprintf(tmp, "Reading %s ...", file_name);
  mvwprintw(backupw, line, 2, tmp);
  wrefresh(backupw);
  member_file = fopen(file_name, "rb");
  size = fread(buffer, 1, 0x800000, member_file);
  prev_tmp = strlen(tmp);
  sprintf(tmp, " Done!");
  mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
  wrefresh(backupw);

  sprintf(tmp, "Uploading ...");
  mvwprintw(backupw, line, 2, tmp);
  wrefresh(backupw);
  if ((rc = ssh_scp_push_file(scp, file_name, size, 0644)) != SSH_OK) {
    // TODO Say so!
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }
  if ((rc = ssh_scp_write(scp, buffer, size)) != SSH_OK) {
    // TODO Say so!
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }
  prev_tmp = strlen(tmp);
  sprintf(tmp, " Done!");
  mvwprintw(backupw, line++, 2 + prev_tmp, tmp);
  wrefresh(backupw);
  mvwprintw(backupw, line, 2, "Backup completed. Press any key.");
  wrefresh(backupw);

  free(buffer);
  ssh_scp_close(scp);
  ssh_disconnect(sshs);
  ssh_free(sshs);
  free(password);
  free(user);
  return 0;
}

static int search_callback(void *vcurr, int argc,
                    char **member, char **colname) {
  int x, y;
  callback_container *cbc = vcurr;

  WINDOW *padw = cbc->padw;
  int *curr = cbc->curr;
  int *curr_line = cbc->curr_line;
  int *visible_members = cbc->visible_members;
  int *delete_rowid = cbc->delete_rowid;
  long RF_TIME = 0L; //-3493586580L;

  getmaxyx(padw, y, x);
  if (*curr == *curr_line)
    wattron(padw, A_REVERSE | A_BOLD);
  mvwprintw(padw, *curr, 2, " %s %s ", member[0], member[1]);
  long ts = strtol(member[2], NULL, 10);
  char *ts_36 = base36(ts);
  mvwprintw(padw, *curr, x - 26, " %s ", atoi(member[3]) ?
            asctime(localtime(&RF_TIME)) : asctime(localtime(&ts)));
  wattron(padw, A_BOLD);
  mvwprintw(padw, *curr, x - 34, " %s ", atoi(member[3]) ?
            " EVIG " : ts_36);
  wattroff(padw, A_BOLD);
  if ((*curr)++ == *curr_line) {
    wattroff(padw, A_REVERSE | A_BOLD);
    *delete_rowid = atoi(member[4]);
  }  (*visible_members)++;
  free(ts_36);
  return 0;
}

int search(sqlite3 *db, WINDOW *main_win, WINDOW *padw,
           char *needle, long period_begin, long period_end,
           int *curr_line, int *visible_members, int *delete_rowid,
           int curr_scroll) {
  *visible_members = 0;
  int y, x, *c;
  c = malloc(sizeof(int));
  *c = 0;

  callback_container curr = {padw, c, curr_line,
                             visible_members, delete_rowid};

  getmaxyx(main_win, y, x);
  werase(padw);
  char *sqls = sqlite3_mprintf("SELECT first_name, last_name, timestamp, \
lifetime, rowid FROM members WHERE first_name || ' ' || last_name \
LIKE '%%%q%%' AND (timestamp > %ld OR lifetime == 1) \
COLLATE NORWEGIAN ORDER BY timestamp DESC", needle, period_begin);
  char *errmsg;
  sqlite3_exec(db, sqls, &search_callback, &curr, &errmsg);
  sqlite3_free(sqls);
  prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
  free(c);
  return 0;
}

char *strstrip(char *str) {
  char *end;
  while (isspace(*str))
    str++;
  if (*str == 0)
    return str;
  end = str + strlen(str) - 1;
  while (end > str && isspace(*end))
    end--;
  *(end + 1) = 0;
  return str;
}

static int stats_callback(void *n, int argc,
                          char **member, char **colnames) {
  (*((int *) n))++;
  return 0;
}

void stats(sqlite3 *db, WINDOW *main_win, int bg, long semstart,
           int price, int w) {
  // TODO This semester so far compared to avg. spring/autumn semester
  int x, y, ch, last24 = 0, thissem = 0;
  int lifetimers = 0, new_lifetimers = 0;
  char sqls[500];
  char tmp[500];

  // Count last 24h
  sprintf(sqls, "SELECT * FROM members WHERE timestamp > %ld \
AND lifetime == 0",
          time(NULL)-86400);
  sqlite3_exec(db, sqls, &stats_callback, &last24, 0);

  // Count this semester
  sprintf(sqls, "SELECT * FROM members WHERE timestamp > %ld",
          semstart);
  sqlite3_exec(db, sqls, &stats_callback, &thissem, 0);

  // Count lifetimers
  sqlite3_exec(db, "SELECT * FROM members WHERE lifetime == 1",
               &stats_callback, &lifetimers, 0);

  // Count new lifetimers
  sprintf(sqls, "SELECT * FROM members WHERE timestamp > %ld \
AND lifetime == 1", semstart);
  sqlite3_exec(db, sqls, &stats_callback, &new_lifetimers, 0);

  if (bg == IN_BACKGROUND) {
    mvprintw(1, COLS - 33, "NOK: %5d TDY: %5d TTL: %5d",
             last24 * price, last24, thissem);
    return;
  }

  getmaxyx(main_win, y, x);
  werase(main_win);
  box(main_win, 0, 0);

  int t = 10;

  sprintf(tmp, "LAST 24 HOURS");
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  sprintf(tmp, "New members:   %8d", last24);
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  sprintf(tmp, "Revenue (NOK): %8d", last24 * price);
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);

  sprintf(tmp, "THIS SEMESTER");
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  sprintf(tmp, "Lifetimers:    %8d", lifetimers);
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  //  sprintf(tmp, "New lifetimers:%8d", new_lifetimers);
  //  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  sprintf(tmp, "New members:   %8d", thissem);
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  sprintf(tmp, "Total members: %8d", thissem + lifetimers);
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);
  sprintf(tmp, "Revenue (NOK): %8d", thissem * price
          + new_lifetimers * price * 10);
  mvwprintw(main_win, t++, (x - strlen(tmp)) >> 1, tmp);

  wrefresh(main_win);
  if (w == NOWAIT)
    return;
  for (;;) {
    switch (ch = getch()) {
    case 27:
      return;
    }
  }
}

void debug(char *msg) {
  mvprintw(0, 0, msg);
  update_panels();
  doupdate();
}

void register_member(sqlite3 *db, WINDOW *main_win, char *f_name,
                     char *l_name, bool lifetime, long ts,
                     long semstart, int price) {
  char sqls[500];
  char *errmsg = 0;
  sprintf(sqls, "INSERT INTO members (first_name, last_name, \
lifetime, timestamp, paid) VALUES ('%s', '%s', %d, %ld, %d)", f_name, l_name,
          (int) lifetime, ts, price);
  sqlite3_exec(db, sqls, 0, 0, &errmsg);
  stats(db, main_win, IN_BACKGROUND, semstart, price, NOWAIT);
}

void read_conf(char *conf_name, int *price, char *domain,
               char *path, char *file_name) {
  FILE *fp = fopen(conf_name, "r");
  fscanf(fp, "%s %s %s %d", domain, path, file_name, price);
  fclose(fp);
}
