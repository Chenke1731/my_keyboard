#ifndef CPS_KEY_H
#define CPS_KEY_H

#include "stdint.h"
#include "keycode.h"
#include "set.hpp"
#include "keyboard.hpp"

typedef struct
{ //希望const会优化一下空间占用
  const uint8_t scan_keys[5];
  const uint8_t scan_key_len;
  const uint8_t trigger_keys[5];
  const uint8_t trigger_key_len;
  const uint8_t key_allow_insert[16];
  const uint8_t key_allow_insert_len;
  bool mode;
  uint8_t matched;
  uint8_t unmatched;
  bool has_triggered;
} composite_key_t;

#define DEFAULT_CPS_BASE .mode = false,      \
                         .matched = 0,   \
                         .unmatched = 0, \
                         .has_triggered = false

extern int composite_key_len;
extern composite_key_t composite_keymap[];

extern uint8_t key_pressed_num;

Set press_trigger_key_set;
Set blocked_press_key_set;
Set recover_key_set;

bool check_in_allow_insert_key_list(composite_key_t *cps_key, uint8_t keycode)
{
  uint8_t i;
  if (cps_key->key_allow_insert_len != 0)
    for (i = 0; i < cps_key->key_allow_insert_len; i++)
      if (cps_key->key_allow_insert[i] == keycode)
        return true;
  return false;
}

int index_of(const uint8_t list[], uint8_t len, uint8_t keycode)
{
  uint8_t i;
  if (len != 0)
    for (i = 0; i < len; i++)
      if (list[i] == keycode)
        return i;
  return -1;
}

void print_key(uint8_t keycode)
{
  Serial.print(" ");
  Serial.print(keycode);
  Serial.print(" ");
}

void remove_key_from_blocked_set(uint8_t keycode)
{
  blocked_press_key_set.remove(keycode);
}

void trigger_composite_key(uint8_t keycode, bool pressed)
{
  uint8_t i, j;
  int scan_key_index;
  bool block = false;
  bool is_short_recover_key = false;
  bool has_trigger = false;

  composite_key_t *cps_key;
  for (i = 0; i < composite_key_len; i++)
  {
    cps_key = &composite_keymap[i];

    if (pressed)
    {
      Serial.print("press: ");
      if (cps_key->scan_keys[cps_key->matched] == keycode)
      {
        Serial.print("scan key");
        cps_key->matched++;
        // 阻塞该默认
        if (cps_key->matched == 1 && cps_key->mode == 1 && cps_key->unmatched == 0) {
          // 取消阻塞
        } else
        if (!(cps_key->matched == 1 && cps_key->unmatched != 0))
        {
          block = true;
          blocked_press_key_set.add(keycode);
        }
        // 判断时候打到触发条件

        Serial.print(" unmatched:");
        Serial.print(cps_key->unmatched);
        Serial.print(" matched:");
        Serial.print(cps_key->matched);
        if (cps_key->matched == cps_key->scan_key_len && cps_key->unmatched == 0)
        {
          // 触发组合键, 一次性按下所有的按键
          for (j = 0; j < cps_key->trigger_key_len; j++)
            if (press_trigger_key_set.add(cps_key->trigger_keys[j]))
              send_key(cps_key->trigger_keys[j], pressed);

          cps_key->has_triggered = true;
          has_trigger = true;
          Serial.print(" and triggered");
          press_trigger_key_set.foreach (print_key);
        }
      }

      // 非scan_key和insert_key
      else if (!check_in_allow_insert_key_list(cps_key, keycode))
      {
        Serial.print("not insert key");
        cps_key->unmatched++;
        // 现在组合键处于未触发状态, 则释放被阻塞的按钮, 会重复触发, 不该在这里触发
        if (cps_key->matched < cps_key->scan_key_len && cps_key->matched > 0)
          for (j = 0; j < cps_key->matched; j++)
          {
            // 把已经match, 那些被阻塞的, 取消阻塞
            recover_key_set.add(cps_key->scan_keys[j]);
            Serial.print(" add to recover set");
          }
      }
      Serial.println();
    }
    else // release
    {
      // 检测是否属于组合键的扫描键
      Serial.print("release: ");
      scan_key_index = index_of(cps_key->scan_keys, cps_key->scan_key_len, keycode);
      if (scan_key_index != -1)
      {
        if (cps_key->scan_keys[cps_key->matched] != keycode && cps_key->unmatched != 0 && !check_in_allow_insert_key_list(cps_key, keycode))
          cps_key->unmatched--;

        if (cps_key->matched != 0)
          cps_key->matched--;

        // 当已经触发过的时候
        if (cps_key->has_triggered)
        {
          // 慢释放
          Serial.print("scan key");
          block = true;
          blocked_press_key_set.remove(keycode);

          Serial.print(" and trigger key");

          j = scan_key_index > cps_key->trigger_key_len - 1
                  ? cps_key->trigger_key_len - 1
                  : scan_key_index;

          for (; j < cps_key->trigger_key_len; j++)
            if (press_trigger_key_set.remove(cps_key->trigger_keys[j]))
              send_key(cps_key->trigger_keys[j], false);

          if (cps_key->matched == 0)
          {
            cps_key->has_triggered = false;
            cps_key->unmatched = 0;
            recover_key_set.empty();
            if (cps_key->mode == 1)
              block = false;
            Serial.print(" all");
            press_trigger_key_set.foreach (print_key);
          }
        }

        // 未触发的时候释放了中间状态
        // 仅仅触发了一个的时候
        else if (key_pressed_num == 1 && cps_key->scan_keys[0] == keycode)
          is_short_recover_key = true;
      }

      // 非scan_key和非insert key
      else if (!check_in_allow_insert_key_list(cps_key, keycode))
      {
        if (cps_key->unmatched != 0)
          cps_key->unmatched--;

        Serial.print("not insert key");
        // 如果减去之后会恢复match的阻塞状态
        if (cps_key->unmatched == 0 && cps_key->matched != 0 && cps_key->mode != 1)
          for (j = 0; j < cps_key->matched; j++)
            if (blocked_press_key_set.add(cps_key->scan_keys[j]))
              release_key(cps_key->scan_keys[j]);
      }
      Serial.println();
    }
  }

  if (!block)
  {
    send_key(keycode, pressed);
  }

  if (is_short_recover_key)
  {
    send_key(keycode, true);
    delay(20);
    send_key(keycode, false);
  }

  if (!has_trigger && pressed)
  {
    recover_key_set.foreach (press_key);
    recover_key_set.foreach (remove_key_from_blocked_set);
    recover_key_set.empty();
  }
}

#endif
