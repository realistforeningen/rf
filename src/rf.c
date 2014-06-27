#define _GNU_SOURCE
#define _X_OPEN_SOURCE_EXTENDED
#include <locale.h>
#include <string.h>
#include <libssh/libssh.h>
#include <stdlib.h>
#include <time.h>
#include <ncurses.h>
#include <form.h>
#include <panel.h>
#include <sqlite3.h>

int isdigit(int ch);
int isspace(int ch);
void destroy_win(WINDOW *local_win);
WINDOW *newwin(int height, int width, int starty, int startx);

bool delete(int dl);
char *strstrip(char *str);
int read_file();
int get_lifetimers();
int csv2reg(char *line);
int *sem2my(char *sem);
void debug(char *msg);
void home();
void members();
void menu();
void printmenu(WINDOW *mw, char **menu_s, int ML, int active);
void reg();
void register_member(char *f_name, char *l_name,
                     bool lifetime, long int ts);
int search(char *needle, int hl);
void stats();
void update_status_line();

const int PRICE = 50;
const char *RF = "R E A L I S T F O R E N I N G E N";
char* file_name= "members.csv";
sqlite3 *db;
int num_members, num_lifetimers, num_members_today;
int curr_line, curr_scroll;
int delete_rowid = 0, visible_members = 0;
long int semstart;
PANEL *panels[5];
WINDOW *main_win, *menu_win, *edit_win, *padw;

int main() {
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
  wrefresh(main_win);
  ESCDELAY = 0;
  mvprintw(0, (COLS - strlen(RF)) / 2, RF);
  mvprintw(1, 2, "Menu  Edit");

  // Define start of semester
  time_t ts_now = time(NULL);
  struct tm *now = gmtime(&ts_now);
  now->tm_hour = 0;
  now->tm_min = 0;
  now->tm_sec = 0;
  now->tm_mday = 1;
  now->tm_mon = now->tm_mon < 6 ? 0 : 6;
  semstart = (long int) mktime(now);

  // Open database
  sqlite3_open("members.db", &db);
  char *errmsg;
  char *sql = "CREATE TABLE IF NOT EXISTS members\
 (first_name NCHAR(50) NOT NULL, last_name NCHAR(50) NOT NULL,\
 timestamp BIGINT NOT NULL)";
  sqlite3_exec(db, sql, NULL, NULL, &errmsg);
  //  debug(errmsg);

  // Load members from file
  //  num_members = read_file();
  update_status_line();

  // Start main loop
  home();
  menu();

  endwin();
  return 0;
}

void menu() {
  int active_y = 0, active_x = 0, i, ch, cur_menu_len;
  char **menu_s, **menu_e;
  const int MENU_LEN = 5, EDIT_MENU_LEN = 3;

  panels[0] = new_panel(main_win);

  menu_s = malloc(sizeof(char*) * MENU_LEN);
  for (i = 0; i < MENU_LEN; i++)
    menu_s[i] = malloc(sizeof(char) * 15);
  menu_s[0] = " Home     ";
  menu_s[1] = " Register ";
  menu_s[2] = " Members  ";
  menu_s[3] = " Stats    ";
  menu_s[4] = " Exit     ";
  menu_e = malloc(sizeof(char*) * EDIT_MENU_LEN);
  for (i = 0; i < EDIT_MENU_LEN; i++)
    menu_e[i] = malloc(sizeof(char) * 15);
  menu_e[0] = " Backup   ";
  menu_e[1] = " N/A      ";
  menu_e[2] = " N/A      ";

  // Windows for roll-down menus
  menu_win = newwin(MENU_LEN + 2, 12, 2, 0);
  panels[1] = new_panel(menu_win);
  edit_win = newwin(EDIT_MENU_LEN + 2, 12, 2, 6);
  panels[4] = new_panel(edit_win);

  padw = newpad(5000, COLS - 2);
  panels[2] = new_panel(padw);
  update_panels();
  doupdate();

  keypad(menu_win, true);
  box(menu_win, 0, 0);
  keypad(edit_win, true);
  box(edit_win, 0, 0);

  for (;;) {
    if (active_x == 0) {
      hide_panel(panels[4]);
      show_panel(panels[1]);
      printmenu(menu_win, menu_s, MENU_LEN, active_y);
      cur_menu_len = MENU_LEN;
    } else {
      hide_panel(panels[1]);
      show_panel(panels[4]);
      printmenu(edit_win, menu_e, EDIT_MENU_LEN, active_y);
      cur_menu_len = EDIT_MENU_LEN;
    }
    update_panels();
    doupdate();

    switch (ch = getch()) {
    case 27:
      return;
    case KEY_RIGHT:
    case KEY_LEFT:
      active_y = 0;
      active_x = active_x == 0 ? 1 : 0;
      break;
    case KEY_UP:
      active_y == 0 ? active_y = cur_menu_len - 1 : active_y--;
      break;
    case KEY_DOWN:
      active_y == cur_menu_len - 1 ? active_y = 0 : active_y++;
      break;
    case 10:
      switch (active_y) {
      case 0:
        active_x ? 0 : home();
        break;
      case 1:
        active_x ? 0 : reg();
        break;
      case 2:
        active_x ? 0 : members();
        break;
      case 3:
        stats();
        break;
      case 4:
        return;
      }
    }
  }
  for (i = 0; i < MENU_LEN; i++)
    free(menu_s[i]);
  free(menu_s);
  for (i = 0; i < EDIT_MENU_LEN; i++)
    free(menu_e[i]);
  free(menu_e);
}

void printmenu(WINDOW *mw, char **menu_s, int ML, int active) {
  int y = 1, x = 1, i;
  box(mw, 0, 0);
  for (i = 0; i < ML; i++) {
    i == active ? wattron(mw, A_REVERSE) : 0;
    mvwprintw(mw, y++, x, menu_s[i]);
    i == active ? wattroff(mw, A_REVERSE) : 0;
  }
}

void home() {
  // TODO dancing bear
  int x, y, ch;
  getmaxyx(main_win, y, x);
  char *s = "Dette er Realistforeningens medlemsliste.";
  werase(main_win);
  box(main_win, 0, 0);
  mvwprintw(main_win, y / 2, (x - strlen(s)) / 2, s);
  wrefresh(main_win);
  for (;;) {
    switch (ch = getch()) {
    case 27:
      return;
    }
  }
}

void reg() {
  int x, y, ch, h = 15, wi = 50, i;
  FIELD *fields[3];
  FORM *rf_form;
  WINDOW *formw, *dwin;
  char *f_name, *l_name;
  hide_panel(panels[1]);
  getmaxyx(main_win, y, x);

  for (i = 0; i < 2; i++) {
    fields[i] = new_field(1, 25, 1 + i * 2, 12, 0, 0);
    set_field_back(fields[i], A_UNDERLINE);
  }
  fields[2] = NULL;

  rf_form = new_form(fields);
  scale_form(rf_form, &h, &wi);
  formw = newwin(h + 3, wi + 4, (y - h) / 2, (x - wi) / 2);
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
  wrefresh(formw);
  curs_set(1);
  for (;;) {
    switch (ch = getch()) {
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
      register_member(f_name, l_name, false, time(NULL));
    case 27:
      hide_panel(panels[3]);
      //      if (ch == 27)
        //        show_panel(panels[1]);
      update_panels();
      doupdate();
      //      prefresh(padw, curr_line, 1, 3, 1, y, x-2);
      unpost_form(rf_form);
      free_form(rf_form);
      curs_set(0);
      for (i = 0; i < 2; i++)
        free_field(fields[i]);
      return;
    default:
      form_driver(rf_form, ch);
      break;
    }
    wrefresh(formw);
  }
}

void members() {
  int x, y, ch;
  hide_panel(panels[1]);
  getmaxyx(main_win, y, x);
  werase(main_win);
  box(main_win, 0, 0);
  update_panels();
  doupdate();

  search("", 0);
  bool search_mode = false;
  char *needle_buf = "", *send_s;
  int needle_idx = 0;
  bool btm = false; // Hack to make scrolling work
  curr_line = 0;
  curr_scroll = 0;
  prefresh(padw, curr_scroll, 1, 3, 1, y, x-2);
  for (;;) {
    switch (ch = getch()) {
    case KEY_DOWN:
      if (curr_line == visible_members - 1) {
        btm = false;
        break;}
      search(needle_buf, ++curr_line);
      if (curr_line == visible_members - 1 && btm)
        break;
      if (curr_line > y - 3)
        prefresh(padw, ++curr_scroll, 1, 3, 1, y, x-2);
      move(1, 27 + needle_idx);
      break;
    case KEY_UP:
      if (curr_line == 0) {
        btm = false;
        break;
      }
      if (curr_line == visible_members - 1)
        btm = true;
      search(needle_buf, --curr_line);
      if (curr_line < curr_scroll)
        prefresh(padw, --curr_scroll, 1, 3, 1, y, x-2);
      move(1, 27 + needle_idx);
      break;
    case KEY_BACKSPACE:
      if (search_mode && needle_idx > 0) {
        needle_buf[--needle_idx] = '\0';
        send_s = (char *) malloc(needle_idx + 1);
        strncpy(send_s, needle_buf, needle_idx+1);
        search(send_s, curr_line = 0);
        free(send_s);
        mvprintw(1, 26, "                         ");
        mvprintw(1, 27, "%s", needle_buf);
      }
      break;
    case KEY_DC: // Delete
      if (!delete(delete_rowid))
        break;
      update_status_line();
      curr_line = 0;
      search(needle_buf, 0);
      num_members--;
      break;
    case 47:
      if (!search_mode) {
        curr_line = 0;
        curr_scroll = 0;
        search_mode = true;
        needle_buf = (char *) malloc(100);
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
        search("", 0);
        free(needle_buf);
        mvprintw(1, 19, "                                ");
        prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
        break;
      }
      curr_line = 0;
      werase(padw);
      prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
      return;
    default:
      //      mvprintw(1, 19,"%d", ch);
      if (search_mode && needle_idx < 32) {
        werase(padw);
        needle_buf[needle_idx++] = (char) ch;
        needle_buf[needle_idx] = '\0';
        send_s = (char *) malloc(needle_idx + 1);
        strncpy(send_s, needle_buf, needle_idx + 1);
        search(send_s, 0);
        free(send_s);
        prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
        mvprintw(1, 26, " %s", needle_buf);
      }
    }
  }
}

bool delete(int dl) {
  char sqls[50];
  sprintf(sqls, "DELETE FROM members WHERE rowid == %d", dl);
  sqlite3_exec(db, sqls, 0, 0, 0);
  return true;
}

static int search_callback(int *curr, int argc,
                           char **member, char **colname) {
  int x, y;
  getmaxyx(main_win, y, x);
  if (*curr == curr_line)
    wattron(padw, A_REVERSE);
  mvwprintw(padw, *curr, 2, "%s %s", member[0], member[1]);
  long int ts = strtol(member[2], NULL, 10);
  mvwprintw(padw, *curr, x - 26, "%s", asctime(localtime(&ts)));
  if ((*curr)++ == curr_line) {
    wattroff(padw, A_REVERSE);
    delete_rowid = atoi(member[3]);
  }
  visible_members++;
  return 0;
}

int search(char *needle, int hl) {
  visible_members = 0;
  char sqls[500];
  int curr = 0, y, x;
  getmaxyx(main_win, y, x);
  werase(padw);
  sprintf(sqls, "SELECT first_name, last_name, timestamp, \
rowid FROM members WHERE (last_name LIKE '%%%s%%' OR first_name \
LIKE '%%%s%%') AND timestamp > %ld ORDER BY timestamp DESC",
needle, needle, semstart);
  sqlite3_exec(db, sqls, &search_callback, &curr, 0);
  prefresh(padw, curr_scroll, 1, 3, 1, y, x - 2);
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

static int stats_callback(int *n, int argc,
                          char **member, char **colnames) {
  (*n)++;
  return 0;
}

void stats() {
  // TODO This semester so far compared to avg. spring/autumn semester
  int x, y, ch, last24 = 0, thissem = 0;
  int lifetimers = 0, new_lifetimers = 0;
  char sqls[500];
  char tmp[500];

  // Count last 24h
  sprintf(sqls, "SELECT * FROM members WHERE timestamp > %ld",
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

  getmaxyx(main_win, y, x);
  werase(main_win);
  box(main_win, 0, 0);

  int t = 10;

  sprintf(tmp, "LAST 24 HOURS");
  mvwprintw(main_win, t, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "New members:   %8d", last24);
  mvwprintw(main_win, t + 1, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "Revenue (NOK): %8d", last24 * PRICE);
  mvwprintw(main_win, t + 2, (x - strlen(tmp)) / 2, tmp);

  sprintf(tmp, "THIS SEMESTER");
  mvwprintw(main_win, t + 5, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "Lifetimers:    %8d", lifetimers);
  mvwprintw(main_win, t + 6, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "New lifetimers:%8d", new_lifetimers);
  mvwprintw(main_win, t + 7, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "New members:   %8d", thissem);
  mvwprintw(main_win, t + 8, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "Total members: %8d", thissem + lifetimers);
  mvwprintw(main_win, t + 9, (x - strlen(tmp)) / 2, tmp);
  sprintf(tmp, "Revenue (NOK): %8d", thissem * PRICE
          + new_lifetimers * PRICE * 10);
  mvwprintw(main_win, t + 10, (x - strlen(tmp)) / 2, tmp);

  wrefresh(main_win);
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

void register_member(char *f_name, char *l_name, bool lifetime, long int ts) {
  // TODO Rewrite to SQL
  char sqls[500];
  char *errmsg = 0;
  sprintf(sqls, "INSERT INTO members (first_name, last_name, \
timestamp) VALUES ('%s', '%s', %ld)", f_name, l_name, ts);
  sqlite3_exec(db, sqls, 0, 0, &errmsg);
  num_members_today++;
  num_members++;
  update_status_line();
}

void update_status_line() {
  mvprintw(1, COLS - 33, "NOK: %5d TDY: %5d TTL: %5d",
           num_members_today * PRICE, num_members_today, num_members);
}

char *strtok2(char *line, char tok) {
  char *tmp = strdup(line);
  int i = 0;
  while (*line != tok && *line != 0) {
    line++;
    i++;
  }
  if (*line == 0)
    return NULL;
  line++;
  return strndup(tmp, i);
}

int *sem2my(char *sem) {
  int *r = malloc(sizeof(int) * 2);
  char s = *sem++;
  r[0] = s == 'h' && s == 'H' ? 6 : 0;
  while (!isdigit(*sem))
    sem++;
  int year = atoi(sem);
  r[1] = year < 70 ? 2000 + year : 1900 + year;
  return r;
}

int csv2reg(char *line) {
  char *comment, *name, *sem, *issued_by;
  int num;
  long int ts = 0;
  char ln[500];

  num = atoi(strtok2(line, ','));
  comment = strtok2(line, ',');
  name = strtok2(line, ',');
  sem = strtok2(line, ',');
  issued_by = strtok2(line, ',');

  if (*sem == 0) {
    struct tm *now = gmtime(0);
    int *my_sem = sem2my(sem);
    now->tm_year = my_sem[1];
    now->tm_mon = my_sem[0];
    ts = (long int) mktime(now);
  }

  sprintf(ln, "(%s)", comment);

  register_member(name, ln, true, ts);

  free(line);
  return 0;
  }

int read_buffer(char *buffer) {
  char *line = NULL;
  int i = 0;
  line = strtok(buffer, "\n");
  while (line != NULL) {
    csv2reg(line);
    line = strtok(NULL, "\n");
    i++;
  }
  return i;
}

int get_lifetimers() {
  ssh_session sshs = ssh_new();
  ssh_scp scp;
  int rc, size, num_lt_members;
  char *password, *buffer;

  if (sshs == NULL)
    return -1;

  // TODO Read host, user from config file
  ssh_options_set(sshs, SSH_OPTIONS_HOST, "login.ifi.uio.no");
  ssh_options_set(sshs, SSH_OPTIONS_USER, "rf");
  rc = ssh_connect(sshs);
  if (rc != SSH_OK) {
    ssh_free(sshs);
    return -1;
  }

  // TODO Authenticate server
  // TODO Read password from user
  password = "***REMOVED***";
  rc = ssh_userauth_password(sshs, NULL, password);
  if (rc != SSH_AUTH_SUCCESS) {
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }

  // TODO Read path from config file
  scp = ssh_scp_new(sshs, SSH_SCP_READ, "Sekretos/.../livstid.csv");
  if (scp == NULL) {
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }
  rc = ssh_scp_init(scp);
  rc = ssh_scp_pull_request(scp);
  size = ssh_scp_request_get_size(scp);
  buffer = malloc(size);
  //  filename = strdup(ssh_scp_request_get_filename(scp));
  //  mode = ssh_scp_request_get_permissions(scp);

  ssh_scp_accept_request(scp);
  rc = ssh_scp_read(scp, buffer, size);

  // TODO Write new 'parseline' to comply with format
  num_lt_members = read_buffer(buffer);

  free(buffer);
  rc = ssh_scp_pull_request(scp);
  if (rc != SSH_SCP_REQUEST_EOF) {
    ssh_disconnect(sshs);
    ssh_free(sshs);
    return -1;
  }
  ssh_disconnect(sshs);
  ssh_free(sshs);
  return num_lt_members;
}
