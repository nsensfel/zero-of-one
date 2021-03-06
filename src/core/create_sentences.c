#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h> /* defines SIZE_MAX */

#include "../io/error.h"

#include "knowledge.h"

/** Functions to create sentences using a ZoO_knowledge structure *************/

/*
 * Returns the index of a element in {links} chosen randomly according
 * to the distribution in {links_occurrences}.
 * Pre:
 *    (!= occurrences 0).
 *    (== (length links_occurrences) (length links)).
 *    (== (sum links_occurrences) occurrences).
 *    (can_store ZoO_index (length links)).
 */
static ZoO_index pick_index
(
   ZoO_index const occurrences,
   const ZoO_index links_occurrences [const restrict static 1]
)
{
   ZoO_index result, accumulator, random_number;

   result = 0;

   /*
    * Safe:
    * (> (length links_occurrences) 0).
    */
   accumulator = links_occurrences[0];

   random_number = (((ZoO_index) rand()) % occurrences);

   while (accumulator < random_number)
   {
      /*
       * Safe:
       * (->
       *    (and
       *       (== accumulator (sum links_occurrences[0:result]))
       *       (< accumulator random_number)
       *       (< random_number occurrences)
       *       (== occurrences (sum links_occurrences))
       *       (can_store ZoO_index (length links))
       *       (== (length links_occurrences) (length links))
       *    )
       *    (and
       *       (< result' (length links_occurrences))
       *       (can_store ZoO_index result')
       *       (=< accumulator' occurrences)
       *    )
       * )
       */
      result += 1;
      accumulator += links_occurrences[result];
   }

   /* Safe: (< result (length links)) */
   return result;
}

static unsigned char * extend_left
(
   struct ZoO_knowledge k [const restrict static 1],
   ZoO_index sequence [const restrict static ZoO_MARKOV_ORDER],
   ZoO_char current_sentence [static 1],
   size_t sentence_size [const restrict static 1],
   ZoO_index credits [const static 1]
)
{
   size_t addition_size;
   struct ZoO_knowledge_word * w;
   ZoO_char * next_sentence;
   ZoO_index j;

   /* prevents current_sentence [restrict] */
   next_sentence = current_sentence;

   for (;;)
   {
      if (*credits == 0)
      {
         return current_sentence;
      }

      *credits -= 1;

      w = (k->words + sequence[ZoO_MARKOV_ORDER - 1]);

      switch (w->special)
      {
         case ZoO_WORD_HAS_NO_EFFECT:
            /* FIXME: not overflow-safe. */
            /* word also contains an '\0', which we will replace by a ' ' */
            addition_size = w->word_size;
            break;

         case ZoO_WORD_ENDS_SENTENCE:
            ZoO_S_WARNING("END OF LINE should not be prefixable.");
            return current_sentence;

         case ZoO_WORD_STARTS_SENTENCE:
            return current_sentence;

         case ZoO_WORD_REMOVES_LEFT_SPACE:
         case ZoO_WORD_REMOVES_RIGHT_SPACE:
            /* word also contains an '\0', which we will remove. */
            addition_size = w->word_size - 1;
            break;
      }

      if (*sentence_size > (SIZE_MAX - addition_size))
      {
         ZoO_S_WARNING
         (
            "Sentence construction aborted to avoid size_t overflow."
         );

         return current_sentence;
      }

      next_sentence =
         (ZoO_char *) calloc
         (
            /* overflow-safe */
            (*sentence_size + addition_size),
            sizeof(ZoO_char)
         );

      if (next_sentence == (ZoO_char *) NULL)
      {
         ZoO_S_ERROR("Could not allocate memory to store new sentence.");

         return current_sentence;
      }

      /* overflow-safe */
      *sentence_size = (*sentence_size + addition_size);

      switch (w->special)
      {
         case ZoO_WORD_HAS_NO_EFFECT:
            snprintf
            (
               next_sentence,
               *sentence_size,
               " " ZoO_CHAR_STRING_SYMBOL ZoO_CHAR_STRING_SYMBOL,
               w->word,
               current_sentence
            );
            break;

         case ZoO_WORD_REMOVES_LEFT_SPACE:
            snprintf
            (
               next_sentence,
               *sentence_size,
               ZoO_CHAR_STRING_SYMBOL ZoO_CHAR_STRING_SYMBOL,
               w->word,
               current_sentence
            );
            break;

         case ZoO_WORD_REMOVES_RIGHT_SPACE:
            snprintf
            (
               next_sentence,
               *sentence_size,
               ZoO_CHAR_STRING_SYMBOL ZoO_CHAR_STRING_SYMBOL,
               w->word,
               /* Safe: strlen(current_sentence) >= 2 */
               (current_sentence + 1)
            );
            break;

         default:
            /* TODO: PROGRAM LOGIC ERROR */
            break;
      }

      free((void *) current_sentence);

      /* prevents current_sentence [const] */
      current_sentence = next_sentence;

      memmove
      (
         (void *) (sequence + 1),
         (const void *) sequence,
         /* Accepts 0. */
         (sizeof(ZoO_index) * (ZoO_MARKOV_ORDER - 1))
      );

      if
      (
         ZoO_knowledge_find_link
         (
            w->backward_links_count,
            w->backward_links,
            (sequence + 1),
            &j
         )
         < 0
      )
      {
         ZoO_S_ERROR("Unexpectedly, no backtracking link was found.");

         break;
      }

      sequence[0] =
         w->backward_links[j].targets
         [
            pick_index
            (
               w->backward_links[j].occurrences,
               w->backward_links[j].targets_occurrences
            )
         ];

      /* prevents current_sentence [const] */
      current_sentence = next_sentence;
   }
}

static unsigned char * extend_right
(
   struct ZoO_knowledge k [const restrict static 1],
   ZoO_index sequence [const restrict static ZoO_MARKOV_ORDER],
   ZoO_char current_sentence [static 1],
   size_t sentence_size [const restrict static 1],
   ZoO_index credits [const static 1]
)
{
   size_t addition_size;
   struct ZoO_knowledge_word * w;
   ZoO_char * next_sentence;
   ZoO_index j;

   /* prevents current_sentence [restrict] */
   next_sentence = current_sentence;

   for (;;)
   {
      if (*credits == 0)
      {
         return current_sentence;
      }

      *credits -= 1;

      w = (k->words + sequence[0]);

      switch (w->special)
      {
         case ZoO_WORD_HAS_NO_EFFECT:
            /* FIXME: Assumed to be overflow-safe. */
            /* word also contains an '\0', which we will replace by a ' '. */
            addition_size = w->word_size;
            break;

         case ZoO_WORD_ENDS_SENTENCE:
            return current_sentence;

         case ZoO_WORD_STARTS_SENTENCE:
            ZoO_S_WARNING("START OF LINE should not be suffixable.");
            return current_sentence;

         case ZoO_WORD_REMOVES_LEFT_SPACE:
         case ZoO_WORD_REMOVES_RIGHT_SPACE:
            /* word also contains an '\0', which we will remove. */
            addition_size = w->word_size - 1;
            break;
      }

      if (*sentence_size > (SIZE_MAX - addition_size))
      {
         ZoO_S_WARNING
         (
            "Sentence construction aborted to avoid size_t overflow."
         );

         return current_sentence;
      }

      next_sentence =
         (ZoO_char *) calloc
         (
            /* overflow-safe */
            (*sentence_size + addition_size),
            sizeof(ZoO_char)
         );

      if (next_sentence == (ZoO_char *) NULL)
      {
         ZoO_S_ERROR("Could not allocate memory to store new sentence.");

         return current_sentence;
      }

      /* overflow-safe */
      *sentence_size = (*sentence_size + addition_size);

      switch (w->special)
      {
         case ZoO_WORD_REMOVES_LEFT_SPACE:
            current_sentence[*sentence_size - addition_size - 2] = '\0';

         case ZoO_WORD_HAS_NO_EFFECT:
            snprintf
            (
               next_sentence,
               *sentence_size,
               ZoO_CHAR_STRING_SYMBOL ZoO_CHAR_STRING_SYMBOL " ",
               current_sentence,
               w->word
            );
            break;

         case ZoO_WORD_REMOVES_RIGHT_SPACE:
            snprintf
            (
               next_sentence,
               *sentence_size,
               ZoO_CHAR_STRING_SYMBOL ZoO_CHAR_STRING_SYMBOL,
               current_sentence,
               w->word
            );
            break;

         default:
            /* TODO: PROGRAM LOGIC ERROR */
            break;
      }

      free((void *) current_sentence);

      /* prevents current_sentence [const] */
      current_sentence = next_sentence;

      memmove
      (
         (void *) sequence,
         (const void *) (sequence + 1),
         /* Accepts 0. */
         (sizeof(ZoO_index) * (ZoO_MARKOV_ORDER - 1))
      );

      if
      (
         ZoO_knowledge_find_link
         (
            w->forward_links_count,
            w->forward_links,
            sequence,
            &j
         )
         < 0
      )
      {
         ZoO_S_ERROR("Unexpectedly, no forward link was found.");

         break;
      }

      sequence[ZoO_MARKOV_ORDER - 1] =
         w->forward_links[j].targets
         [
            pick_index
            (
               w->forward_links[j].occurrences,
               w->forward_links[j].targets_occurrences
            )
         ];
   }
}

static ZoO_index select_first_word
(
   struct ZoO_knowledge k [const static 1],
   const struct ZoO_strings string [const],
   ZoO_index const aliases_count,
   const char * restrict aliases [const restrict static aliases_count]
)
{
   ZoO_index i, j, word_id, word_min_score, word_min_id;
   ZoO_index word_found;

   if (string == (struct ZoO_strings *) NULL)
   {
      return word_min_id = (rand() % k->words_count);
   }

   word_found = 0;

   for (i = 0; i < string->words_count; ++i)
   {
      for (j = 0; j < aliases_count; ++j)
      {
         if (ZoO_IS_PREFIX(aliases[j], string->words[i]))
         {
            goto NEXT_WORD;
         }
      }

      /* prevents k [restrict] */
      if (ZoO_knowledge_find(k, string->words[i], &word_min_id) == 0)
      {
         word_found = 1;
         word_min_score = k->words[word_min_id].occurrences;

         break;
      }

      NEXT_WORD:;
   }

   if (word_found == 0)
   {
      return word_min_id = (rand() % k->words_count);
   }

   for (; i < string->words_count; ++i)
   {
      for (j = 0; j < aliases_count; ++j)
      {
         if (ZoO_IS_PREFIX(aliases[j], string->words[i]))
         {
            goto NEXT_WORD_BIS;
         }
      }

      if
      (
         (ZoO_knowledge_find(k, string->words[i], &word_id) == 0)
         && (k->words[word_id].occurrences < word_min_score)
      )
      {
         word_min_score = k->words[word_id].occurrences;
         word_min_id = word_id;
      }

      NEXT_WORD_BIS:;
   }

   return word_min_id;
}

static void init_sequence
(
   struct ZoO_knowledge k [const static 1],
   const struct ZoO_strings string [const],
   ZoO_index const aliases_count,
   const char * restrict aliases [const restrict static aliases_count],
   ZoO_index sequence [const static (ZoO_MARKOV_ORDER * 2) + 1]
)
{
   ZoO_index i, j, accumulator, random_number;
   struct ZoO_knowledge_word * fiw;

   sequence[ZoO_MARKOV_ORDER] =
      select_first_word(
         k,
         string,
         aliases_count,
         aliases
      );

   fiw = (k->words + sequence[ZoO_MARKOV_ORDER]);

   for (i = 0; i < ZoO_MARKOV_ORDER; ++i)
   {
      sequence[ZoO_MARKOV_ORDER - i - 1] = ZoO_WORD_START_OF_LINE;
      sequence[ZoO_MARKOV_ORDER + i + 1] = ZoO_WORD_END_OF_LINE;
   }

   if (fiw->forward_links_count == 0)
   {
      ZoO_S_FATAL("First word has no forward links.");

      return;
   }

   /* Chooses a likely forward link for the pillar. */
   i = 0;
   accumulator = fiw->forward_links[0].occurrences;

   random_number = (((ZoO_index) rand()) % fiw->occurrences);

   while (accumulator < random_number)
   {
      accumulator += fiw->forward_links[i].occurrences;
      i += 1;
   }

/*   i = (((ZoO_index) rand()) % fiw->forward_links_count); */

   /* Copies the forward link data into the sequence. */
   /* This adds (ZoO_MARKOV_ORDER - 1) words, as the ZoO_MARKOV_ORDERth word */
   /* is chosen aftewards. */
   memcpy
   (
      (void *) (sequence + ZoO_MARKOV_ORDER + 1),
      fiw->forward_links[i].sequence,
      sizeof(ZoO_index) * (ZoO_MARKOV_ORDER - 1)
   );

   /* selects the last word */
   sequence[ZoO_MARKOV_ORDER * 2] =
      fiw->forward_links[i].targets
      [
         pick_index
         (
            fiw->forward_links[i].occurrences,
            fiw->forward_links[i].targets_occurrences
         )
      ];

   /* FIXME: Not clear enough. */
   /* Now that we have the right side of the sequence, we are going to */
   /* build the left one, one word at a time. */
   for (i = 0; i < ZoO_MARKOV_ORDER; ++i)
   {
      /* temporary pillar (starts on the right side, minus one so we don't */
      fiw = (k->words + sequence[(ZoO_MARKOV_ORDER * 2) - i - 1]);

      if
      (
         /* finds the backward link corresponding to the words left of the */
         /* temporary pillar. */
         ZoO_knowledge_find_link
         (
            fiw->backward_links_count,
            fiw->backward_links,
            sequence + (ZoO_MARKOV_ORDER - i),
            &j
         )
         < 0
      )
      {
         ZoO_ERROR
         (
            "Unexpectedly, no back link was found at i=%u, expected to find"
            "a backlink with %s, from %s.",
            i,
            k->words[sequence[(ZoO_MARKOV_ORDER - i)]].word,
            fiw->word
         );
         ZoO_S_ERROR("Sequence was:");

         for (j = 0; j <= (ZoO_MARKOV_ORDER * 2); ++j)
         {
            ZoO_ERROR("[%u] %s", j, k->words[sequence[j]].word);
         }

         break;
      }

      sequence[ZoO_MARKOV_ORDER - i - 1] =
         fiw->backward_links[j].targets
         [
            pick_index
            (
               fiw->backward_links[j].occurrences,
               fiw->backward_links[j].targets_occurrences
            )
         ];
   }
}

int ZoO_knowledge_extend
(
   struct ZoO_knowledge k [const static 1],
   const struct ZoO_strings string [const],
   ZoO_index const aliases_count,
   const char * restrict aliases [const restrict static aliases_count],
   ZoO_char * result [const static 1]
)
{
   int word_found;
   size_t sentence_size;
   ZoO_index sequence[(ZoO_MARKOV_ORDER * 2) + 1];
   ZoO_index first_word, credits;

   credits = ZoO_MAX_REPLY_WORDS;

   init_sequence(k, string, aliases_count, aliases, sequence);

   first_word = sequence[ZoO_MARKOV_ORDER];

   /* 3: 2 spaces + '\0' */
   /* FIXME: not overflow-safe */
   switch (k->words[first_word].special)
   {
      case ZoO_WORD_REMOVES_LEFT_SPACE:
      case ZoO_WORD_REMOVES_RIGHT_SPACE:
         /* word + ' ' + '\0' */
         sentence_size = (strlen(k->words[first_word].word) + 2);
         break;

      case ZoO_WORD_HAS_NO_EFFECT:
         /* word + ' ' * 2 + '\0' */
         sentence_size = (strlen(k->words[first_word].word) + 3);
         break;

      default:
         ZoO_WARNING
         (
            "'%s' was unexpectedly selected as pillar.",
            k->words[first_word].word
         );
         /* word + '[' + ']' + ' ' * 2 + '\0' */
         sentence_size = (strlen(k->words[first_word].word) + 5);
         break;
   }

   *result = (ZoO_char *) calloc(sentence_size, sizeof(ZoO_char));

   if (*result == (ZoO_char *) NULL)
   {
      ZoO_S_ERROR("Could not allocate memory to start sentence.");

      return -2;
   }

   switch (k->words[first_word].special)
   {
      case ZoO_WORD_REMOVES_LEFT_SPACE:
         snprintf
         (
            *result,
            sentence_size,
            ZoO_CHAR_STRING_SYMBOL " ",
            k->words[first_word].word
         );
         break;

      case ZoO_WORD_REMOVES_RIGHT_SPACE:
         snprintf
         (
            *result,
            sentence_size,
            " " ZoO_CHAR_STRING_SYMBOL,
            k->words[first_word].word
         );
         break;

      case ZoO_WORD_HAS_NO_EFFECT:
         snprintf
         (
            *result,
            sentence_size,
            " " ZoO_CHAR_STRING_SYMBOL " ",
            k->words[first_word].word
         );
         break;

      default:
         snprintf
         (
            *result,
            sentence_size,
            " [" ZoO_CHAR_STRING_SYMBOL "] ",
            k->words[first_word].word
         );
         break;
   }

   /* prevents result [restrict] */
   *result =
      extend_right
      (
         k,
         (sequence + ZoO_MARKOV_ORDER + 1),
         *result,
         &sentence_size,
         &credits
      );

   if (*result == (ZoO_char *) NULL)
   {
      return -2;
   }

   *result =
      extend_left
      (
         k,
         sequence,
         *result,
         &sentence_size,
         &credits
      );

   if (*result == (ZoO_char *) NULL)
   {
      return -2;
   }

   return 0;
}
