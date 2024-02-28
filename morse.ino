#include "driver/timer.h"

void IRAM_ATTR morse_on_timer();
class Morse_ {
public:
  static constexpr int max_durations = 1000;
  static constexpr int ms = 1000;
  static constexpr int s = 1000000;
  static constexpr int max_hist_len = 16;

  bool buffer_ready;
  bool buffer_processed;
  int prev_input;
  hw_timer_t* timer;
  int64_t last_change_time;
  int64_t start_time;
  int64_t stop_time;
  int silent_max;
  int debounce_wait;
  int64_t debounce_time;
  int durations[max_durations];
  int durations_len;
  int shortest;
  int longest;
  int time_unit;
  int input_type;
  int input_pin;
  int output_pin;
  bool negate_input;
  char tokens[max_durations + 1];
  int token_count;
  char result[max_durations];
  int histogram[max_hist_len];
  int bucket_boundaries[max_hist_len + 2];  // more precisely, top boundaries of histogram buckets. Starts implied.
  int bins[3];
  int bin_counts[3];

  Morse_()
    : buffer_ready(false),
      buffer_processed(true),
      prev_input(0),
      timer(NULL),
      last_change_time(0),
      start_time(0),
      stop_time(0),
      silent_max(2 * s),
      debounce_wait(10 * ms),
      debounce_time(0),
      durations_len(0),
      shortest(0),
      longest(0),
      time_unit(250 * ms),
      input_type(INPUT_PULLUP),
      input_pin(0),
      output_pin(-1),
      negate_input(-1),
      token_count(0){};

  ~Morse_() {
    timerStop(timer);
    timerDetachInterrupt(timer);
    timerEnd(timer);
  }

  static constexpr char* morse_code[] = {
    "",        /* NUL */
    "",        /* SOH */
    "",        /* STX */
    "",        /* ETX */
    "",        /* EOT */
    "",        /* ENQ */
    "",        /* ACK */
    "",        /* BEL */
    "......",  /* BS */
    "",        /* HT */
    ".-.-",    /* LF */
    "",        /* VT */
    "",        /* FF */
    "",        /* CR */
    "",        /* SO */
    "",        /* SI */
    "",        /* DLE */
    "",        /* DC1 */
    "",        /* DC2 */
    "",        /* DC3 */
    "",        /* DC4 */
    "",        /* NAK */
    "",        /* SYN */
    "",        /* ETB */
    "",        /* CAN */
    "",        /* EM */
    "",        /* SUB */
    "",        /* ESC */
    "",        /* FS */
    "",        /* GS */
    "",        /* RS */
    "",        /* US */
    "/",       /*   */
    "-.-.--",  /* ! */
    ".-..-.",  /* " */
    "",        /* # */
    "...-..-", /* $ */
    "",        /* % */
    ".-...",   /* & */
    ".----.",  /* ' */
    "-.--.",   /* ( */
    "-.--.-",  /* ) */
    "",        /* * */
    ".-.-.",   /* + */
    "--..--",  /* , */
    "-....-",  /* - */
    ".-.-.-",  /* . */
    "-..-.",   /* / */
    "-----",   /* 0 */
    ".----",   /* 1 */
    "..---",   /* 2 */
    "...--",   /* 3 */
    "....-",   /* 4 */
    ".....",   /* 5 */
    "-....",   /* 6 */
    "--...",   /* 7 */
    "---..",   /* 8 */
    "----.",   /* 9 */
    "---...",  /* : */
    "-.-.-.",  /* ; */
    "",        /* < */
    "-...-",   /* = */
    "",        /* > */
    "..--..",  /* ? */
    ".--.-.",  /* @ */
    ".-",      /* A */
    "-...",    /* B */
    "-.-.",    /* C */
    "-..",     /* D */
    ".",       /* E */
    "..-.",    /* F */
    "--.",     /* G */
    "....",    /* H */
    "..",      /* I */
    ".---",    /* J */
    "-.-",     /* K */
    ".-..",    /* L */
    "--",      /* M */
    "-.",      /* N */
    "---",     /* O */
    ".--.",    /* P */
    "--.-",    /* Q */
    ".-.",     /* R */
    "...",     /* S */
    "-",       /* T */
    "..-",     /* U */
    "...-",    /* V */
    ".--",     /* W */
    "-..-",    /* X */
    "-.--",    /* Y */
    "--..",    /* Z */
    "",        /* [ */
    "",        /* \ */
    "",        /* ] */
    "",        /* ^ */
    "..--.-",  /* _ */
    "",        /* ` */
    ".-",      /* a */
    "-...",    /* b */
    "-.-.",    /* c */
    "-..",     /* d */
    ".",       /* e */
    "..-.",    /* f */
    "--.",     /* g */
    "....",    /* h */
    "..",      /* i */
    ".---",    /* j */
    "-.-",     /* k */
    ".-..",    /* l */
    "--",      /* m */
    "-.",      /* n */
    "---",     /* o */
    ".--.",    /* p */
    "--.-",    /* q */
    ".-.",     /* r */
    "...",     /* s */
    "-",       /* t */
    "..-",     /* u */
    "...-",    /* v */
    ".--",     /* w */
    "-..-",    /* x */
    "-.--",    /* y */
    "--..",    /* z */
    "",        /* { */
    "",        /* | */
    "",        /* } */
    "",        /* ~ */
    "",        /* DEL */
  };



  void begin() {
    timer = timerBegin(1000000);
    timerAttachInterrupt(timer, &morse_on_timer);
    timerAlarm(timer, 20 * ms, true, 0);  // 50ms
    if (output_pin >= 0) {
      pinMode(input_pin, OUTPUT);
    }
    if (input_pin >= 0) {
      pinMode(input_pin, input_type);
      // With pullup, key pressed reads as 0, so negate the read
      negate_input = (input_type == INPUT_PULLUP);
    }
  };

  int64_t micros64() {
    static uint32_t low32, high32;
    int32_t new_low32 = micros();
    if (new_low32 < low32) high32++;
    low32 = new_low32;
    return ((int64_t)high32 << 32) | low32;
  };

  void ReadMorse() {
    // Buffer processing in progress, ignore inputs.
    if (!buffer_processed) return;
    if (durations_len >= max_durations) {
      // Buffer full. Flush what you got.
      buffer_ready = true;
      buffer_processed = false;
      return;
    }

    buffer_ready = false;
    int change_time;
    int inp = abs(digitalRead(input_pin) - negate_input);

    if (inp != prev_input) {
      // Input changed.
      change_time = micros64();
      prev_input = inp;

      // debounce
      if (change_time - debounce_time < debounce_wait) {
        debounce_time = change_time;
        return;
      }

      int delta = change_time - last_change_time;
      if (inp) {
        // keydown = register prior silence.

        if (delta > silent_max) {
          // ignore initial silence.
          last_change_time = change_time;
          //          Serial.println("Ignore silence");
          return;
        }
        //        Serial.println("Keydown");
        durations[durations_len++] = delta;  // register silence duration
      } else {
        //keyup = signal completed.

        //ignore tail end of reset longpress;
        if (delta > silent_max && durations_len == 0) {
          return;
        }

        //        Serial.println("Keyup");
        durations[durations_len++] = delta;  // register signal duration
      }
      last_change_time = change_time;
    } else {
      // no button state change
      if (inp) {
        //longpress as first input makes no sense
        if (durations_len == 0) return;
        // long press = reset
        if (micros64() > last_change_time + silent_max) {
          reset();
          //          Serial.println("Reset");
        }
      } else {
        // initial silence, no input = do nothing.
        if (durations_len == 0) return;
        // final silence for 2s = done, ready to parse.
        if (micros64() > last_change_time + silent_max) {
          //          Serial.println("Ready");
          buffer_ready = true;
          buffer_processed = false;
        }
      }
    }
  };

  void reset() {
    durations_len = 0;
    buffer_ready = false;
    buffer_processed = true;
  }

  // Creates a 16-bucket histogram and 3-bucket binned averages.

  void create_histogram_and_bins() {
    // find the range of durations
    int bins[3] = { 0 };
    int bin_counts[3] = { 0 };
    memset(histogram, 0, sizeof(histogram));
    memset(bucket_boundaries, 0, sizeof(bucket_boundaries));

    // find the bucket size
    int hl = max_hist_len - 2;  // we want to leave first and last bucket empty, to make median easier.
    {
      int range = longest - shortest;                 // Total range of the data
      int bucket = range / (hl - 1);                  // Size of each bucket
      bucket_boundaries[0] = longest - 1.5 * bucket;  //start at half the bucket below lowest
      for (int i = 1; i < max_hist_len + 2; ++i) {
        // last boundary half the bucket below highest.
        bucket_boundaries[i] = bucket_boundaries[i - 1] + bucket;
      }
    }

    for (int i = 0; i < durations_len; ++i) {
      int duration = durations[i];
      for (int j = 0; j < max_hist_len; ++j) {
        if (duration < bucket_boundaries[j]) {
          ++histogram[j - 1];
          if (j < 6) {
            bins[0] += duration;
            ++bin_counts[0];
          } else if (j < 11) {
            bins[1] += duration;
            ++bin_counts[1];
          } else {
            bins[2] += duration;
            ++bin_counts[2];
          }
          break;
        }
      }
    }

    if (bin_counts[1] == 0) {
    }
    for (int i = 0; i < 3; ++i) {
      if (bin_counts[i] > 0) {
        bins[i] /= bin_counts[i];
      }
    }
  }

  inline void swap(int* x, int* y) {
    *x ^= *y;
    *y ^= *x;
    *x ^= *y;
  }

  inline int middle3(int a, int b, int c) {
    return a > b ? (c > a ? a : (b > c ? b : c)) : (c > b ? b : (a > c ? a : c));
  }

  void median() {
    int backup[max_hist_len];
    memcpy(backup, histogram, sizeof(histogram));
    for (int i = 1; i < max_hist_len - 1; ++i) {
      histogram[i] = middle3(backup[i - 1], backup[i], backup[i + 1]);
    }
  }

  inline int find_bucket(int v) {
    if (v < bucket_boundaries[6]) return 0;
    if (v < bucket_boundaries[11]) return 1;
    return 2;
  }

  int find_smallest(int* arr, int len, bool zeroallowed = true) {
    int smallest = 2147483647;
    for (int i = 0; i < len; ++i) {
      if (arr[i] < smallest && (zeroallowed || arr[i] > 0)) {
        smallest = arr[i];
      }
      return smallest;
    }
  }
  int find_biggest(int* arr, int len) {
    int biggest = -2147483648;
    for (int i = 0; i < len; ++i) {
      if (arr[i] > biggest) {
        biggest = arr[i];
      }
      return biggest;
    }
  }

  void append_token(const char t) {
    if (token_count < max_durations) {
      tokens[token_count++] = t;
    }
  }
  void append_null() {
    tokens[token_count] = '\0';
  }

  void append_tokens(const char* t) {
    int l = strlen(t);
    if (token_count + l < token_count) {
      strcpy((tokens + token_count), t);
      token_count += l;
    }
  }

  // The input is too short or too uniform to decide what it contains on its own merits;
  // We're using whatever heuristics will help us determine what it is, primarily time_unit estimate.

  void hinted_discriminator() {
    token_count = 0;
    // one character type only;
    if (longest < 2 * shortest) {
      if (longest + shortest / 2 < time_unit)  // only dots/intra
      {
        for (int i = 0; i < durations_len; ++i) {
          if (!(i & 1)) continue;  // skip intra-character spaces
          append_token('.');
        }
      } else  // only dashes and spaces
      {
        for (int i = 0; i < durations_len; ++i) {
          if (!(i & 1)) {
            append_token(' ');
          } else {
            append_token('-');
          }
        }
      }
      append_null();
      return;
    }

    // There are at least two types of entries;

    if (bins[1] == 0) {
      int avg = (bins[0] + bins[2]) / 2;
      if (bins[2] < 5.5 * time_unit) {  // Dots and dashes
        for (int i = 0; i < durations_len; ++i) {
          if (durations[i] < avg) {
            if (!(i & 1)) continue;
            append_token('.');
          } else {
            if (!(i & 1)) {
              append_token(' ');
            } else {
              append_token('-');
            }
          }
        }
      } else if (bins[0] > 1.5 * time_unit) {  // Dashes and spaces
        for (int i = 0; i < durations_len; ++i) {
          if (durations[i] < avg) {
            if (!(i & 1)) {
              append_token(' ');
            } else {
              append_token('-');
            }
          } else {
            if (!(i & 1)) {
              append_tokens(" / ");
            } else {
              append_token('-');
            }
          }
        }
      } else {  // Dots and spaces
        for (int i = 0; i < durations_len; ++i) {
          if (durations[i] < avg) {
            if (!(i & 1)) continue;
            append_token('.');
          } else {
            if (!(i & 1)) {
              append_tokens(" / ");
            } else {
              append_token('-');
            }
          }
        }
      }
      append_null();
      return;
    }

    // There are three types of entries
    for (int i = 0; i < durations_len; ++i) {
      int b = find_bucket(durations[i]);
      switch (b) {
        case 0:
          if (!(i & 1)) break;
          append_token('.');
          break;
        case 1:
          if (!(i & 1)) {
            append_token(' ');
          } else {
            append_token('-');
          }
          break;
        case 2:
        default:
          if (!(i & 1)) {
            append_tokens(" / ");
          } else {
            append_token('-');
          }
          break;
      }
    }
    append_null();
    return;
  }

  void sink() {
    int smallest_land = find_smallest(histogram, max_hist_len, false);
    for (int i = 0; i < max_hist_len; ++i) {
      histogram[i] -= smallest_land;
    }
  }


  // Perform analysis of input to determine time unit.

  bool adjust_tu() {
    int isles = 0;
    int isle_starts[3];
    int isle_ends[3];

    median();
    median();
    median();

    while (1) {
      isles = 0;
      bool was_on_isle = false;  // on isle?
      for (int i = 0; i < max_hist_len; ++i) {
        bool am_on_isle = histogram[i] > 0;
        if (was_on_isle != am_on_isle) {
          if (am_on_isle) {
            isle_starts[isles] = i;
            isles++;
            if (isles == 4) break;
          } else {
            isle_ends[isles] = i;
          }
          was_on_isle = am_on_isle;
        }
      }
      // if we have one landmass, sink and hope to split it into 2-3.
      // if we have many small islets, sink the smallest of them.
      if (isles == 1 || isles > 3) {
        sink();
      } else {
        break;  // hopefully 2 or 3, but 0 can happen too.
      }
    }

    if (isles == 0)  // ouch, we've overdone sinking and drowned!
    {
      return false;
    }

    // Use the average binning buckets to gather only the records we are confident about
    memset(bins, 0, sizeof(bins));
    memset(bin_counts, 0, sizeof(bin_counts));

    for (int i = 0; i < durations_len; ++i) {
      int duration = durations[i];
      for (int j = 0; j < isles; ++j) {
        if (duration >= bucket_boundaries[isle_starts[j]] && duration <= bucket_boundaries[isle_ends[j]]) {
          bins[j] += duration;
          ++bin_counts[j];
          break;
        }
      }
    }

    for (int j = 0; j < isles; ++j) {
      if (bin_counts[j] > 0) {
        bins[j] /= bin_counts[j];
        break;
      }
    }

    // Estimate Time Unit from the bins -
    // bin[0]'s avg is 1tu, bin[1]'s avg is 3tu, bin[2] is 7tu. Weight the average by count of records.
    int found_tu = 0;
    if (isles == 2) {
      if (bins[0] > 1.5 * time_unit)  // dashes and spaces
      {
        found_tu = ((bins[0] / 3) * bin_counts[0] + (bins[1] / 7) * bin_counts[1])
                   / (bin_counts[0] + bin_counts[1]);
      } else  // dots and dashes
      {
        found_tu = (bins[0] * bin_counts[0] + (bins[1] / 3) * bin_counts[1])
                   / (bin_counts[0] + bin_counts[1]);
      }
    } else  // isles == 3
    {
      found_tu = (bins[0] * bin_counts[0] + (bins[1] / 3) * bin_counts[1] + (bins[2] / 7) * bin_counts[2])
                 / (bin_counts[0] + bin_counts[1] + bin_counts[2]);
    }
    // adjust TU
    time_unit = found_tu * 0.4 + time_unit * 0.6;
    return true;
  }

  // Simple tokenizer based on TU
  void tokenize() {
    token_count = 0;
    int thresh1 = time_unit * 1.5;
    int thresh2 = time_unit * 5.5;
    for (int i = 0; i < durations_len; ++i) {
      int duration = durations[i];
      if (duration < thresh1) {
        if (!(i & 1)) continue;
        append_token('.');
      } else if (duration < thresh2) {
        if (!(i & 1)) {
          append_token(' ');
        } else {
          append_token('-');
        }
      } else {
        if (!(i & 1)) {
          append_tokens(" / ");
        } else {
          append_token('-');
        }
      }
    }
    append_null();
  }

  void visualize(int* data, int len) {
    for (int i = 0; i < 10; i++) {
      Serial.println("-10,10,0");
    }
    for (int i = 0; i < len; i++) {
      int v = data[i] / 10000;
      for (int j = 0; j < v; j++) {
        Serial.print("-10,10,");
        Serial.println((!(i & 1)));
      }
    }
    for (int i = 0; i < 10; i++) {
      Serial.println(0);
    }
  }

  void parseMorse() {
    char* res = result;
    char* saveWord;
    char* saveLetter;
    char* word = strtok_r(tokens, "/", &saveWord);
    while (word != NULL) {
      char* letter = strtok_r(word, " ", &saveLetter);
      while (letter != NULL) {
        for (int i = 0; i < sizeof(morse_code)-1; ++i) {
          if (strcmp(letter, morse_code[i]) == 0) {
            *res = (char)i;
            res++;
            break;
          }
        }
        letter = strtok_r(NULL, " ", &saveLetter);
      }
      *res = ' ';
      res++;
      word = strtok_r(NULL, "/", &saveWord);
    }
    *res = '\0';
  }

  bool ready() {
    return buffer_ready;
  }

  void ack() {
    reset();
    Serial.println("Ack");
  }

  char* getParsed() {

    if (!buffer_ready) return "";
    // both approaches require these
    //visualize(durations, durations_len);
    shortest = find_smallest(durations, durations_len);
    longest = find_biggest(durations, durations_len);
    create_histogram_and_bins();

    // If data is good enough to produce adjusted time unit, we can use a simple tokenizer.
    if (durations_len > 6 && longest >= shortest * 2 && adjust_tu()) {
      tokenize();
    } else {
      // Data not good enough. Fall back to heuristic approach
      hinted_discriminator();
    }
    Serial.println(tokens);
    parseMorse();
    durations_len = 0;
    return result;
  }
};

Morse_ Morse;

void IRAM_ATTR morse_on_timer() {
  Morse.ReadMorse();
};



void setup() {
  Serial.begin(115200);
  Morse.begin();
}

void loop() {
  if (Morse.ready()) {
    Serial.println(Morse.getParsed());
    Morse.ack();
  } else {
    delay(100);
  }
}
