
/*
// TODO :
- recursive call in sub-projects
- test failing target
- Fix windows command line quoting !
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>

#define PATHSIZE (4096) // TODO : get maximum path size for the system

#define HASH_EMPTY    "nohash"
#define TIME_EMPTY    "notime"

#define LID_UT_CAT2(A,B) A ## B
#define LID_UT_CAT(A,B) LID_UT_CAT2(A,B)
#define LID(N) LID_UT_CAT(__,LID_UT_CAT(N,__LINE__))

// To be used in a single line, e.g.: STATICF(char *var, "%d", 1);
#define STATICF(C, L, F, ...) \
  static char LID(x)[L];                            \
  snprintf(LID(x), sizeof(LID(x)), F, __VA_ARGS__); \
  C = LID(x)

// To be used in a single line, e.g.: STACKF(char *var, "%d", 1);
#define STACKF(C, F, ...) \
  int LID(x) = snprintf(NULL, 0, F, __VA_ARGS__);   \
  char LID(y)[LID(x)+1];                            \
  snprintf(LID(y), sizeof(LID(y)), F, __VA_ARGS__); \
  C = LID(y)

struct {
  char* rebuild;
  char* database;
  char* builder;
  char* prefix;
  char* default_target;
  char* parent_target;
  char* nesting;
  char* sequence;
  int check_time;
  int check_hash;
  int verbosity;
} opt = {0};

#define info(L,F,...) do{ \
  if(L <= 0 || opt.verbosity >= L) {\
    fprintf(L<0 ? stderr : stdout, "rebuild %s " F, opt.nesting, __VA_ARGS__); \
    fflush(L<0 ? stderr : stdout); \
  } \
}while(0)

#define die(F,...) do{ \
  info(-1, F, __VA_ARGS__); \
  exit(-1); \
}while(0)

static char line_buffer[PATHSIZE +16 +8 +2 +4 +1]; // path:PATHSIZE timestamp:16 hash:8 type:1 spaces:3 terminator:1

// system dependent hooks
#define ERROR_PROCESS_EXECUTION (-1001)
static char* get_current_directory();
static char* get_process_binary(int argc, char* argv[]);
static int set_environment_variable(const char* key, const char* value);
static int run_child(const char * cmd);
static int make_directory(const char * path);
static int file_exists(const char * name);
static int is_file_executable(const char * path);
static int move_file(const char * from, const char * to);
static char* get_modification_time(const char* path);

// in the cli section
static int set_environment_for_subprocess(const char* target);

static int make_parent_directory(const char* path){
  char * sep = (char *) path;
  while(1){
    sep = strchr(sep, '/');
    if(!sep) break;
    if (path != sep){
      *(char*)sep = '\0';
      int result = make_directory(path);
      *(char*)sep = '/';
      if (result) return result;
    }
    sep += 1;
  }
  return 0;
}

#define TARGET_PREFIX(T) ((T) == opt.parent_target ? "" : opt.prefix)
#define FINAL_PATH(P, T) STACKF(P, "%s%s", TARGET_PREFIX(T), T)

#define DB_PATH(C,T,L) STACKF(C, "%s/%s%s%s", opt.database, TARGET_PREFIX(T), T, \
  ((L) == 0 ? "_dep.txt" : (L) == 1 ? "_dep.txt.wip" : "_dep.txt.wip.wip"))

#define DB_MARK_PATH(C,T) STACKF(C, "%s/%s%s%s", opt.database, TARGET_PREFIX(T), T, "_outofdate.txt")

static void db_mark_rebuild(const char* target, int to_rebuild){
  DB_MARK_PATH(char* path, target);
  if (!to_rebuild) remove(path);
  else {
    FILE* f = fopen(path, "wb");
    if (f) fclose(f);
  }
}

static int db_should_rebuild(const char* target){
  DB_MARK_PATH(char* path, target);
  return file_exists(path);
}

static int db_clear(const char* target){
  char* path;
  DB_PATH(path, target, 1);
  remove(path);
  DB_PATH(path, target, 0); // TODO : cleanup, do not allocate new space on stack
  info(7, "clear [%s] [/wip].\n", path);
  remove(path);
  return 0;
}

static int is_in_db(const char* target, int level){
  DB_PATH(char* path, target, level);
  return file_exists(path);
}

static int db_commit(const char* target, int level, int report){
  DB_PATH(char* from, target, level+1);
  DB_PATH(char* to, target, level);
  info(7, "commit [%s] <- [%s].\n", to, from);
  int result = move_file(from, to);
  if (report && result) info(-1, "can not move '%s' to '%s'.\n", from, to); // TODO : is this needed ?
  return result;
}

static FILE* db_open(const char* target, int level, const char* mode, int report){
  DB_PATH(char* path, target, level);
  FILE* result = NULL;
  errno = 0;
  if (!make_parent_directory(path)){
    info(7, "cursor [%s]\n", path);
    errno = 0;
    result = fopen(path, mode);
  }
  if (report && NULL == result) info(-1, "can not open database '%s' - '%s'.\n", target, strerror(errno)); // TODO : is this needed ?
  return result;
}

#undef DB_PATH

static int is_source_file(const char * target){
  FINAL_PATH(char* path, target);
  if (!file_exists(path)) return 0;
  if (is_in_db(target, 0)) return 0;
  return 1;
}

static void check_dependency_cycle(const char* target){
  FINAL_PATH(char* target_path, target);
  int is_cycle = 0;
  char* l = (char*) opt.sequence;
  for (char* s = l; !is_cycle && '\0' != *s; s +=1){
    if ('\n' == *s){
      if (l != s && '\0' != *l){
        *s = '\0';
        if (!strcmp(l, target_path)) is_cycle = 1;
        *s = '\n';
      }
      l = s + 1;
    }
  }
  if (is_cycle) die("target '%s' generates a dependency cycle.\n", target_path);
}

static int run_child_or_die(const char* cmd){
  int result = run_child(cmd);
  if (ERROR_PROCESS_EXECUTION == result)
    die("can not create sub-process - %s.\n", cmd);
  return result;
}

static int rebuild_target(char* target){
  int result = 0;
  if (is_source_file(target)) return 0;
  db_clear(target);
  if (!is_file_executable(opt.builder))
    die("invalid builder '%s'.\n", opt.builder);
  info(1, "running builder script for %s.\n", target);
  //check_dependency_cycle(target);
  db_mark_rebuild(target, 1); // OPT 1
  set_environment_for_subprocess(target);
  if (0 == run_child_or_die(opt.builder)){
    db_mark_rebuild(target, 0); // OPT 1
  } else {
    result = -1;
    //remove(target); // OPT 2 - This should suffice to handle failing build instead
    //                // of the mark 1/0 system; however some issue can arise if
    //                // the removing fails (very unlikely).
  }
  if (!is_in_db(target, 1)) db_clear(target);
  else db_commit(target, 0, 0);
  if (result) die("failed to build target '%s'.\n", target);
  return result;
}

static size_t get_line_from_file(char *s, int size, FILE *stream){
  if (NULL == fgets(s, size, stream)) return 0;
  size_t len = strlen(s);
  if ('\n' == s[len-1]) s[len-1] = '\0';
  return len;
}

typedef enum { UNKNOWN_DEP = -1, CHANGE_DEP = 1, CREATE_DEP, } dependency_type;

static char* format_dependency_line(dependency_type type, const char * hash, const char * time, const char * dependency){
  char* ts = "?";
  switch(type){
    break; default: ;
    break; case CHANGE_DEP: ts = "+";
    break; case CREATE_DEP: ts = "-";
  }
  snprintf(line_buffer, sizeof(line_buffer), "%s %s %s %s", ts, hash, time, dependency);
  return line_buffer;
}

static dependency_type parse_dependency_line(char* line, char** path, char** time, char** hash){
  int do_split = (time!=NULL || hash!=NULL);
  char* tmp = line;

  tmp = strchr(tmp, ' ');
  if (!tmp) goto err;
  dependency_type result = UNKNOWN_DEP;
  switch(line[0]){
    break; case '+': result = CHANGE_DEP;
    break; case '-': result = CREATE_DEP;
  }
  if (do_split) *tmp = '\0';
  tmp += 1;

  if (hash) *hash = tmp;
  tmp = strchr(tmp, ' ');
  if (!tmp) goto err;
  if (do_split) *tmp = '\0';
  tmp += 1;

  if (time) *time = tmp;
  tmp = strchr(tmp, ' ');
  if (!tmp) goto err;
  if (do_split) *tmp = '\0';
  tmp += 1;

  if (path) *path = tmp;
  return result;

err:
  die("invalid dependency file '%s'.\n", *path);
}

static int store_create_dependence(const char* dependency){
  FINAL_PATH(char* dep_path, dependency);
  if (file_exists(dep_path)) die("unexpected error %d\n", __LINE__); // TODO : is this needed ?
  FILE * dep = db_open(opt.parent_target, 1, "a+b", 1);
  if (NULL == dep) die("can not store creation dependency for '%s'.\n", dep_path);
  fseek(dep, 0, SEEK_SET);
  int found = 0;
  char * rp;
  while (!found && get_line_from_file(line_buffer, sizeof(line_buffer), dep))
    if (CREATE_DEP == parse_dependency_line(line_buffer, &rp, NULL, NULL))
      found = !strcmp(rp, dep_path);
  if (!found) fprintf(dep, "%s\n", format_dependency_line(CREATE_DEP, HASH_EMPTY, TIME_EMPTY, dep_path));
  fclose(dep);
  return 0;
}

static int rebuild_if_create(char* target){
  if ('\0' == opt.parent_target[0])
    die("%s", "empty parent target, 'ifcreate' should be called by the builder only.\n");
  store_create_dependence(target);
  return 0;
}

static char * get_timestamp(const char * path){
  if (!opt.check_hash) return TIME_EMPTY; //return "1000-10-10T01:01:01.000001";
  char* result = get_modification_time(path);
  if (NULL == result) die("can not get info about '%s'.\n", path);
  return result;
}

static char* get_hash(const char * path){
  if (!opt.check_hash) return HASH_EMPTY;
  FILE *file = fopen(path, "r");
  if (file == NULL) die("can not open file '%s' - %s.\n", path, strerror(errno));
  
  // Jenkins One-at-time Hash
  uint32_t hash = 0;
  for (int c; EOF != (c = fgetc(file)); ) {
	  hash += (char) c;
	  hash += hash << 10;
	  hash ^= hash >> 6;
  }
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;

  STATICF(char* strhash, 9, "%08x", hash);
  return strhash;
}

static int did_file_change(char* path, char* time, char* hash){
  if (!opt.check_time && !opt.check_hash)
    return 1;
  char* fs_time = get_timestamp(path);
  if (opt.check_time)
   if (strcmp(time, fs_time))
    { info(2, "timestamp of [%s] changed from [%s] to [%s].\n", path, time, fs_time); return 1; }
  char* fs_hash = get_hash(path);
  if (opt.check_hash)
   if (strcmp(hash, fs_hash))
    { info(2, "hash of [%s] changed from [%s] to [%s].\n", path, hash, fs_hash); return 1; }
  return 0;
}

static int rebuild_target_if_needed(const char* target){
  FINAL_PATH(char* target_path, target);
  // TODO : REFACTOR !
  info(1, "for '%s' checking prerequisites of '%s'.\n", opt.parent_target, target_path);
  int should_be_rebuilt = 0;
  if (db_should_rebuild(target_path)) should_be_rebuilt = 6;
  if (!file_exists(target_path)) should_be_rebuilt = 5;
  FILE * file = db_open(target, 0, "rb", 0);
  if (NULL != file){
    size_t len = 0;
    // TODO : check file errors
    fseek(file, 0, SEEK_END);
    len = ftell(file);
    fseek(file, 0, SEEK_SET);
    char cont[len+1];
    fread(cont, len, 1, file);
    cont[len] = '\0';
    fclose(file);
    char* line = cont;
    while (line < cont + sizeof(cont) -1) {
      char* nl = strchr(line, '\n');
      if (nl) *nl = '\0';
      info(7, "dependency found [%s] <- [%s].\n", target, line);
      char *path, *time, *hash;
      if (CHANGE_DEP == parse_dependency_line(line, &path, &time, &hash)){
        if (rebuild_target_if_needed(path))
          should_be_rebuilt = 1;
        if (0< strlen(path)) {
          if (!file_exists(path)) should_be_rebuilt = 2;
          else if (did_file_change(path, time, hash)) should_be_rebuilt = 3;
        }
      }
      if (nl) *nl = '\n';
      if (!nl) break;
      line = nl + 1;
    }
  }
  file = db_open(target, 0, "rb", 0);
  if (file){
    while (get_line_from_file(line_buffer, sizeof(line_buffer), file)) {
      char *path, *time, *hash;
      if (CREATE_DEP == parse_dependency_line(line_buffer, &path, &time, &hash)){
        info(7, "dependency found [%s] <- [%s].\n", target, path);
        if (file_exists(path)) {
          should_be_rebuilt = 4;
          break;
        }
      }
    }
    fclose(file);
  }
  if (0 == should_be_rebuilt) info(1, "'%s' is up to date.\n", target);
  else info(1, "'%s' should be rebuilt (%d).\n", target, should_be_rebuilt); // TODO : proper explanation
  if (should_be_rebuilt) {
    check_dependency_cycle(target);
    set_environment_for_subprocess(target);
    return run_child_or_die(opt.rebuild);
  }
  return 0;
}

static int store_change_dependence(const char * dependency){
  FINAL_PATH(char* dep_path, dependency);
  char * when = TIME_EMPTY;
  char * hash = HASH_EMPTY;
  if (file_exists(dep_path)){
    when = get_timestamp(dep_path);
    hash = get_hash(dep_path);
  }
  FILE * in = db_open(opt.parent_target, 1, "rb", 0);
  FILE * out = db_open(opt.parent_target, 2, "wb", 1);
  if (NULL == out) die("can not store change dependency for '%s'.\n", opt.parent_target);
  if (in){
    fseek(in, 0, SEEK_SET);
    while (get_line_from_file(line_buffer, sizeof(line_buffer), in)) {
      char * rp;
      if (CHANGE_DEP == parse_dependency_line(line_buffer, &rp, NULL, NULL))
        if (strcmp(rp, dep_path)) fprintf(out, "%s\n", line_buffer);
    }
    fclose(in);
  }
  char* content = format_dependency_line(CHANGE_DEP, hash, when, dep_path);
  fprintf(out, "%s\n", content);
  info(7, "dependency stored [%s] (wip.wip) <- [%s].\n", opt.parent_target, content);
  fclose(out);
  if (db_commit(opt.parent_target, 1, 1))
    die("can not commit changes to the dependency database for the target '%s'.\n", opt.parent_target);
  return 0;
}

static int rebuild_if_change(char* target){
  if ('\0' == opt.parent_target[0])
    die("%s", "empty parent target, 'ifchange' should be called by the builder only.\n");
  int result = 0;
  if (rebuild_target_if_needed(target))
    result = -1;
  store_change_dependence(target);
  return result;
}

int cli_main(int argc, char *argv[]) {
  int result = 0;
  char* deftar = opt.default_target;
  if (!strcmp(deftar,"")) deftar = "all";
  if (argc < 2){
	  result = rebuild_target(deftar);
  } else if (strcmp(argv[1], "ifcreate") == 0) {
    argc -= 2;
    argv += 2;
    for(int t = 0; t < argc; t += 1)
      result = rebuild_if_create(argv[t]) || result;
	} else {
	  if (strcmp(argv[1], "ifchange") == 0) {
	    argc -= 2;
	    argv += 2;
      for(int t = 0; t < argc; t += 1)
        result = rebuild_if_change(argv[t]) || result;
	  } else {
	    if (strcmp(argv[1], "target") == 0) {
	      argc -= 2;
	      argv += 2;
	    } else {
        argc -= 1;
        argv += 1;
      }
      for(int t = 0; t < argc; t += 1)
        result = rebuild_target(argv[t]) || result;
	  }
	}
  return result;
}

// --------------------------------------------------------------------------------
// Environment handling

// Exported for the builder
#define ENVAR_REBUILD "REBUILD"
#define ENVAR_TARGET "TARGET"
#define ENVAR_OUTPUT "OUTPUT"

// Rebuild option
#define ENVAR_CHECK_TIME "REBUILD_CHECK_TIME"
#define ENVAR_CHECK_HASH "REBUILD_CHECK_HASH"
#define ENVAR_VERBOSITY "REBUILD_VERBOSITY"

// Execution tracking
#define ENVAR_BUILDER "REBUILD_BUILDER"
#define ENVAR_SEQUENCE "REBUILD_SEQUENCE"
#define ENVAR_DATABASE "REBUILD_DATABASE"
#define ENVAR_PREFIX "REBUILD_PREFIX"

static int print_help(char* cmd){
  printf("REcursive BUILD system - rebuild 0.0.1\n");
  printf("\n");
  printf("Usage:\n");
  printf("  %s target [TARGET 1] [TARGET 2] [...]\n", cmd);
  printf("  %s ifcreate [TARGET 1] [TARGET 2] [...]\n", cmd);
  printf("  %s ifchange [TARGET 1] [TARGET 2] [...]\n", cmd);
  printf("\n");
  printf("Moreover\n");
  printf("  %s\n", cmd);
  printf("is equivalent to\n");
  printf("  %s target all\n", cmd);
  printf("while\n");
  printf("  %s [TARGET 1] [TARGET 2] [...]\n", cmd);
  printf("is equivalent to\n");
  printf("  %s target [TARGET 1] [TARGET 2] [...]\n", cmd);
  printf("\n");
  printf("Option are passed via the following environment variables\n");
  printf("%s\n%s\n%s\n%s\n", ENVAR_BUILDER, ENVAR_CHECK_TIME, ENVAR_CHECK_HASH,
         ENVAR_VERBOSITY);
  printf("\n");
  printf("The following variables are automatically set by the rebuild system:\n");
  printf("%s\n%s\n%s\n%s\n%s\n%s\n", ENVAR_REBUILD, ENVAR_DATABASE, ENVAR_TARGET,
         ENVAR_OUTPUT, ENVAR_SEQUENCE, ENVAR_PREFIX); // ENVAR_TARGET is used too
  return 0;
}

static char * environment_with_default(const char* varname, char* fallback){
  char * result = getenv(varname);
  return result && '\0' != result[0] ? result : fallback;
}

static int set_environment_for_subprocess(const char* target){
  set_environment_variable(ENVAR_DATABASE, opt.database);
  set_environment_variable(ENVAR_PREFIX, opt.prefix);
  set_environment_variable(ENVAR_TARGET, target);
  set_environment_variable(ENVAR_OUTPUT, target); // TODO : remove ? use ENVAR_TARGET instad ?
  set_environment_variable(ENVAR_REBUILD, opt.rebuild);
  set_environment_variable(ENVAR_BUILDER, opt.builder);
  FINAL_PATH(char* target_path, target);
  STACKF(char* seq, "%s%s\n", opt.sequence, target_path);
  set_environment_variable(ENVAR_SEQUENCE, seq);
  return 0;
}

int env_main(int argc, char *argv[]) {

  char* cd = get_current_directory();
  STACKF(cd, "%s/", cd);
#define NEED_PREF(X) ((X)[0] == '.' ? cd : "") // TODO : better check !

  opt.rebuild = get_process_binary(argc, argv);

  opt.database = environment_with_default(ENVAR_DATABASE, "./.rebuild");
  STACKF(opt.database, "%s%s", NEED_PREF(opt.database), opt.database);

  opt.builder = environment_with_default(ENVAR_BUILDER, "./build.cmd");
  STACKF(opt.builder, "%s%s", NEED_PREF(opt.builder), opt.builder);
  
  opt.sequence = environment_with_default(ENVAR_SEQUENCE, "");
  STACKF(opt.sequence, "%s", opt.sequence);

#undef NEED_PREF

  int nesting = 0;
  opt.parent_target = opt.sequence;
  for (char *s = opt.sequence; '\0' != s[0]; s += 1) {
    if ('\n' != s[0]) continue;
    nesting += 1;
    if ('\0' == s[1]) {
      break;
    }
    opt.parent_target = s + 1;
  }
  STACKF(opt.parent_target, "%s", opt.parent_target);
  if ('\0' != opt.parent_target[0])
    opt.parent_target[strlen(opt.parent_target)-1]='\0';

  opt.default_target = environment_with_default(ENVAR_TARGET, "");
  STACKF(opt.default_target, "%s", opt.default_target);

  opt.prefix = environment_with_default(ENVAR_PREFIX, "");
  STACKF(opt.prefix, "%s", opt.prefix);

  char nestr[nesting+2];
  memset(nestr, '-', nesting+1);
  nestr[nesting+1] = '\0';
  opt.nesting = nestr;

  char *astr = environment_with_default(ENVAR_CHECK_TIME, "1");
  opt.check_time = strcmp(astr, "0");

  astr = environment_with_default(ENVAR_CHECK_HASH, "1");
  opt.check_hash =  strcmp(astr, "0");

  astr = environment_with_default(ENVAR_VERBOSITY, "0");
  if (astr[0] < '0' || astr[0] > '9') opt.verbosity = 1;
  else opt.verbosity = astr[0] - '0';

  if (argc > 1)
    if (strcmp(argv[1], "help") == 0
    || strcmp(argv[1], "--help") == 0
    || strcmp(argv[1], "-h") == 0 )
      return print_help(argv[0]);

	return cli_main(argc, argv);
}

// ---------------------------------------------------------------------------------
// system dependent

#if _POSIX_C_SOURCE >= 200809L

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

static char* get_current_directory(){
  return getenv("PWD");
}

static char* get_process_binary(int argc, char* argv[]){
  return argv[0]; // TODO : proper implementation !
}

static int set_environment_variable(const char* key, const char* value){
  return setenv(key, value, 1);
}

static int run_child(const char * cmd){
  pid_t pid = fork();
  if (pid < 0) return ERROR_PROCESS_EXECUTION;
  if (!pid){
    // child - never return
    execl(cmd, cmd, (char *)0);
  } else {
    // parent
    int status;
    if (-1 == waitpid(pid, &status, 0)) return ERROR_PROCESS_EXECUTION;
    int result = 0;
    if (WIFEXITED(status)) result = WEXITSTATUS(status);
    return result;
  }
  return 0;
}

static int make_directory(const char * path){
  return (0 != mkdir(path, 0777) && errno != EEXIST);
}

static int file_exists(const char * name){
  return access(name, F_OK) == 0;
}

static int is_file_executable(const char * path){
  struct stat sb;
  if (stat(path, &sb) == 0 && sb.st_mode & S_IXUSR) return 1;
  return 0;
}

static int move_file(const char * from, const char * to){
  return rename(from, to);
}

static char* get_modification_time(const char* path){
	struct stat st;
  int fd = open(path, 0);
  if (0> fd) return NULL;
	fstat(fd, &st);
  close(fd);
	STATICF(char* timestamp, 17, "%08" PRIx32 "%08" PRIx32, (uint32_t)st.st_mtim.tv_sec, (uint32_t)st.st_mtim.tv_nsec);
	//STATICF(char* timestamp, 17, "%08" PRIx32, (uint32_t)st.st_mtime);
  return timestamp;
  // TODO : return rfc3339 ???
}

#elif _WIN32

#include <windows.h>

#define CPTRW(PTRW, PTRC) \
  int LID(s) = MultiByteToWideChar(CP_ACP, 0, (char*)PTRC, -1, NULL, 0);   \
  WCHAR LID(x)[LID(s)];                                                    \
  MultiByteToWideChar(CP_ACP, 0, (char*)PTRC, -1, (LPWSTR)LID(x), LID(s)); \
  PTRW = LID(x)

static char* get_current_directory(){
  return getenv("CD");
}

static char* get_process_binary(int argc, char* argv[]){
  return argv[0]; // TODO : proper implementation !
}

static int set_environment_variable(const char* key, const char* value){
  SetEnvironmentVariable(key, value);
  return 0;
}

static int run_child(const char * cmd){
  STARTUPINFOW si;
  PROCESS_INFORMATION pi;
  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  ZeroMemory( &pi, sizeof(pi) );
  char *p=cmd;
  for (char*p=cmd;*p!='\0';p++)if(*p=='/')*p='\\';
  STACKF(char* cmlin, "c:\\Windows\\System32\\cmd.exe /C \"%s\"", cmd); // TODO : FIX THIS: correct quoting !
  CPTRW(WCHAR* cmlinW, cmlin);
  if(!CreateProcessW(NULL, cmlinW,
                     NULL, NULL, FALSE, 0, NULL, NULL, &si,  &pi)
  ) return ERROR_PROCESS_EXECUTION;
  WaitForSingleObject( pi.hProcess, INFINITE );
  DWORD exit_code;
  if (FALSE == GetExitCodeProcess(pi.hProcess, &exit_code)) exit_code = -1;
  CloseHandle( pi.hProcess );
  CloseHandle( pi.hThread );
  return exit_code;
}

static int make_directory(const char * path){
  if (path[0]=='\0') return 0;
  if (path[1]==':' && path[2] == '\0') return 0;
  CPTRW(WCHAR* pathW, path);
  int result = !CreateDirectoryW(pathW, NULL);
  if (result && ERROR_ALREADY_EXISTS == GetLastError()) result = 0;
  return result;
}

static int file_exists(const char * name){
  CPTRW(WCHAR* nameW, name);
  DWORD dwAttrib = GetFileAttributesW(nameW);
  return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

static int is_file_executable(const char * path){
  // CPTRW(WCHAR* pathW, path);
  // DWORD additional_info;
  // return GetBinaryTypeW(pathW, &additional_info);
  return 1;
}

static int move_file(const char * from, const char * to){
  remove(to);
  errno = 0;
  CPTRW(WCHAR* fromW, from);
  CPTRW(WCHAR* toW, to);
  int result = !MoveFileW(fromW, toW);
  if (result) result = GetLastError();
  return result;
}

static char* get_modification_time(const char* path){
  CPTRW(WCHAR* pathW, path);
  HANDLE filehandle;
  FILETIME timeinfo;
  filehandle = CreateFileW(pathW, GENERIC_READ, FILE_SHARE_READ,  NULL,  OPEN_EXISTING,  FILE_ATTRIBUTE_NORMAL, NULL);
  if(filehandle == INVALID_HANDLE_VALUE) return NULL;
  if(!GetFileTime(filehandle, NULL, NULL, &timeinfo)) return NULL;
  STATICF(char* timestamp, 17, "%08" PRIx32 "%08" PRIx32, (uint32_t)timeinfo.dwHighDateTime, (uint32_t)timeinfo.dwLowDateTime);
  return timestamp;
}

#else
# error "only posix >= 2008 or windows are supported"
#endif

int main(int argc, char *argv[]) { return env_main(argc, argv); }

