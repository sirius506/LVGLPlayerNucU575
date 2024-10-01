#include "DoomPlayer.h"
#include "ntshell.h"
#include "ntopt.h"
#include "ntstdio.h"
#include "ntlibc.h"
#include "board_if.h"
#include "task.h"

typedef int (*USRCMDFUNC)(int argc, char **argv);

typedef struct {
  char *cmd;
  USRCMDFUNC func;
  char *desc;
} cmd_table_t;

SEMAPHORE_DEF(conread_sem)
SEMAPHORE_DEF(conwrite_sem)

static DOOM_UART_Handle *shell_uart;

static int date_command(int argc, char **argv);
static int time_command(int argc, char **argv);
static int ps_command(int argc, char **argv);
static int flash_command(int argc, char **argv);
static int help_command(int argc, char **argv);

static const cmd_table_t cmdlist[] = {
  { "date",  date_command,  "Usage: date [YY-MM-DD]" },
  { "flash", flash_command, "list flash files" },
  { "ps",    ps_command,    "shows task status" },
  { "time",  time_command,  "Usage: time [HH:MM:SS]" },
  { "help",  help_command,  "shows available commands" },
};

static const char *estate_string[] = {
  "Running",
  "Ready",
  "Blocked",
  "Suspended",
  "Deleted",
  "Invalid",
};

void read_comp(DOOM_UART_Handle *shell_uart)
{

  osSemaphoreRelease(shell_uart->recv_sem);
}

void write_comp(DOOM_UART_Handle *shell_uart)
{

  osSemaphoreRelease(shell_uart->send_sem);
}

static int cons_read(char *buf, int cnt, void *extobj)
{
  DOOM_UART_Handle *shell_uart = (DOOM_UART_Handle *)extobj;

  Board_Uart_Receive_IT(shell_uart, (uint8_t *)buf, 1);
  osSemaphoreAcquire(shell_uart->recv_sem, osWaitForever);
  return cnt;
}

static int cons_write(const char *buf, int cnt, void *extobj)
{
  DOOM_UART_Handle *shell_uart = (DOOM_UART_Handle *)extobj;

  Board_Uart_Transmit_DMA(shell_uart, (uint8_t *)buf, cnt);
  osSemaphoreAcquire(shell_uart->send_sem, osWaitForever);
  return cnt;
}

static int usercmd_callback(int argc, char **argv, void *extobj)
{
  UNUSED(extobj);
  if (argc == 0)
    return 0;

  const cmd_table_t *p = &cmdlist[0];

  for (unsigned int i = 0; i < sizeof(cmdlist) / sizeof(cmdlist[0]); i++)
  {
    if (ntlibc_strcmp((const char *)argv[0], p->cmd) == 0)
    {
      return p->func(argc, argv);
    }
    p++;
  }
  cons_write("???\r\n", 5, shell_uart);
  return 0;
}

static int user_callback(const char *text, void *extobj)
{
  UNUSED(extobj);
  return ntopt_parse(text, usercmd_callback, 0);
}

static int date_command(int argc, char **argp)
{
  char tb[30];
  int slen;

  if (argc > 1)
  {
    argp++;
    slen = ntlibc_strlen(*argp);
    if (slen == 8 && argp[0][2] == '-' && argp[0][5] == '-')
    {
      bsp_set_date(*argp);
    }
  }

  bsp_get_date(tb);
  cons_write(tb, ntlibc_strlen(tb), shell_uart);
  return 0;
}

static int time_command(int argc, char **argp)
{
  char tb[30];
  int slen;

  if (argc > 1)
  {
    argp++;
    slen = ntlibc_strlen(*argp);
    if (slen == 8 && argp[0][2] == ':' && argp[0][5] == ':')
    {
      bsp_set_time(*argp);
    }
  }
  
  bsp_get_time(tb);
  cons_write(tb, ntlibc_strlen(tb), shell_uart);
  return 0;
}

#define MAX_DP_TASK	10

TaskStatus_t DPTaskTable[MAX_DP_TASK];

static int ps_command(int argc, char **argp)
{
  UNUSED(argc);
  UNUSED(argp);
  int i;
  size_t runtime;
  TaskStatus_t *ptask;
  char wb[60];

  ptask = DPTaskTable;
  i = uxTaskGetSystemState(ptask, MAX_DP_TASK, &runtime);
  while (i > 0)
  {
    lv_snprintf(wb, sizeof(wb), "%s %d %s %d\r\n", ptask->pcTaskName, ptask->uxCurrentPriority, estate_string[ptask->eCurrentState], ptask->usStackHighWaterMark);
    cons_write(wb, ntlibc_strlen(wb), shell_uart);
    i--;
    ptask++;
  }

  return 0;
}

static int help_command(int argc, char **argp)
{
  UNUSED(argc);
  UNUSED(argp);
  const cmd_table_t *p = &cmdlist[0];
  char wb[60];

  for (unsigned int i = 0; i < sizeof(cmdlist) / sizeof(cmdlist[0]); i++)
  {
    lv_snprintf(wb, sizeof(wb), "%s:  %s\r\n", p->cmd, p->desc);
    cons_write(wb, ntlibc_strlen(wb), shell_uart);
    p++;
  }
  return 0;
}

static int flash_command(int argc, char **argp)
{
  UNUSED(argc);
  UNUSED(argp);
  int i;
  FS_DIRENT *dirent;
  QSPI_DIRHEADER *dirInfo = (QSPI_DIRHEADER *)QSPI_FLASH_ADDR;
  char wb[60];
  uint32_t last_addr = 0;

  dirent = dirInfo->fs_direntry;

  for (i = 1; i < NUM_DIRENT; i++)
  {
    if (dirent->foffset == 0xFFFFFFFF || dirent->foffset == 0)
      break;

    lv_snprintf(wb, sizeof(wb), "%s %ld @ %lx\r\n", dirent->fname, dirent->fsize, dirent->foffset);
    cons_write(wb, ntlibc_strlen(wb), shell_uart);
    if (dirent->foffset >= last_addr)
      last_addr = dirent->foffset + dirent->fsize;

    dirent++;
  }
  lv_snprintf(wb, sizeof(wb), "%d KB used.\r\n", (int) last_addr / 1024);
  cons_write(wb, ntlibc_strlen(wb), shell_uart);
  return 0;
}

void StartShellTask(void *argp)
{
  UNUSED(argp);
  ntshell_t nts;

  shell_uart = HalDevice.shell_uart;

  shell_uart->recv_sem = osSemaphoreNew(1, 0, &attributes_conread_sem);
  shell_uart->send_sem = osSemaphoreNew(1, 0, &attributes_conwrite_sem);

  shell_uart->uartrx_comp = read_comp;
  shell_uart->uarttx_comp = write_comp;
  Board_Uart_Init(shell_uart);

  ntshell_init(&nts, cons_read, cons_write, user_callback, (void*)shell_uart);
  ntshell_set_prompt(&nts, ">");
  while (1)
  {
    ntshell_execute(&nts);
  }
}
