#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "py/runtime.h"
#include "py/mphal.h"
#include "py/objstr.h"
#include "py/builtin.h"
#include "dcfurs.h"

const char *dcfurs_banner =
"    ____  ____________                   ____            __\n"
"   / __ \\/ ____/ ____/_  ____________   / __ )____ _____/ /___ ____\n"
"  / / / / /   / /_  / / / / ___/ ___/  / __  / __ `/ __  / __ `/ _ \\\n"
" / /_/ / /___/ __/ / /_/ / /  _\\_ \\   / /_/ / /_/ / /_/ / /_/ /  __/\n"
"/_____/\\____/_/    \\__,_/_/  /____/  /_____/\\__,_/\\__,_/\\__, /\\___/\n" 
"                                                      /_____/\n";

/* Some challenge text to be embedded into the firmware */
static const unsigned char challenge_data[] __attribute__((used)) = {
#include "challenge_data.h"
};

#define ESC_NORMAL  "\e[0m"
#define ESC_BOLD    "\e[1m"

/* Lazy implementation to generate random-ish bytes. */
unsigned int
dcfurs_rand(void) {
    static uint32_t seed = 500;
    seed *= 1103515245;
    seed += 12345;
    return seed;
}

#if 0
static void print_shuffle(const char *str)
{
    unsigned int len = strlen(str);
    unsigned int i;

    /* Spit out some random characters */
    for (i = 0; i < len; i++) {
        printf("%c", 0x20 + (dcfurs_rand() % 94));
    }
    mp_hal_delay_ms(100);
    printf("%c", '\r');
    while (*str != '\0') {
        mp_hal_delay_ms(25);
        printf("%c", *str++);
    }
    printf("%c", '\n');
}
#endif

static void print_credit(const char *name, const char *detail)
{
    printf("   " ESC_BOLD "%12s" ESC_NORMAL " - %s\n", name, detail);
}

void dcfurs_emote(const char *str)
{
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t mod = mp_import_name(MP_QSTR_emote, mp_const_none, 0);
        mp_obj_t fun = mp_load_attr(mod, MP_QSTR_render);
        mp_call_function_1(fun, mp_obj_new_str(str, strlen(str)));
        nlr_pop();
    }
}

mp_obj_t dcfurs_credits(void)
{
    /* Make extra sure that the linker doesn't discard the challenge data. */
    asm volatile ("" :: "r"(challenge_data));

    dcfurs_emote("^.^");

    /* Show the Banner */
    printf(ESC_BOLD "%s" ESC_NORMAL, dcfurs_banner);
    printf("\n");

    printf("Brought to you by the dedication and hard work from:\n");
    print_credit("Foobar",      "Electronics design and manufacturing");
    print_credit("Alofoxx",     "Business, logistics and software");
    print_credit("DranoTheCat", "Graphics design and interactive animations");
    print_credit("Kayfox",      "Software and logistics");
    print_credit("Liquidthex",  "Animations and the web animator tool");
    print_credit("FizzOtter",   "Electronics manufacturing and logistics");
    print_credit("Merch Minion", "Graphics, swag and printing");
    print_credit("Jippen",      "Badge challenge and puzzles");
    print_credit("redyoshi49q", "Badge challenge and puzzles");

    printf("\nStay fuzzy, and happy hacking!\n\n");
    return mp_const_none;
}

mp_obj_t dcfurs_eula(void)
{
    dcfurs_emote("@.@");

    printf(ESC_BOLD "PRIVACY POLICY\n" ESC_NORMAL);
    printf("--------------\n");
    printf("We\'ve updated our privacy policy. This is purely out of the goodness of our\n");
    printf("hearts, and has nothing to do with any hypothetical unions on any particular\n");
    printf("continents. Please read every part of this policy carefully, and don\'t just\n");
    printf("skip ahead looking for challenge clues.\n");
    printf("\n");

    printf("This policy governs your interractions with this badge, herein referred to as\n");
    printf("\"the service,\" \"the badge,\" or \"the boop badge,\" and with all other badges\n");
    printf("and hardware of any kind. The enumeration in this policy, of certain rights,\n");
    printf("shall not be construed as a license to resell this badge on eBay. By using\n");
    printf("this service, you opt-in to hosting the fursuit lounge in your home.\n");
    printf("\n");

    printf(ESC_BOLD "YOUR PERSONAL INFORMATION\n" ESC_NORMAL);
    printf("-------------------------\n");
    printf("Please don\'t send us your personal information. We do not want your personal\n");
    printf("information. We have a hard enough time keeping track of our own information,\n");
    printf("let alone yours.\n");
    printf("\n");

    printf("If you tell us your name, internet handle, fursona, or any other identifying\n");
    printf("information, we will forget it immediately. The next time we see you, we\'ll\n");
    printf("struggle to remember who you are, and try desperately to get through the\n");
    printf("conversation so we can go online and hopefully figure it out.\n");
    printf("\n");

    printf(ESC_BOLD "TRACKING PIXELS, COOKIES and BEACONS\n" ESC_NORMAL);
    printf("------------------------------------\n");
    printf("This badge uses pixels in order to form text and animations, some of which\n");
    printf("may remain in your memory after you have turned off the badge. We use cookies\n");
    printf("to encourage you to roll over, shake a paw, and go for walks. We may use\n");
    printf("beacons to start a howl.\n");
    printf("\n");

    printf(ESC_BOLD "3rd PARTY EXTENSIONS\n" ESC_NORMAL);
    printf("--------------------\n");
    printf("The Saturday night party has been extended, and will continue until midnight.\n");
    printf("\n");

    printf(ESC_BOLD "PERMISSIONS\n" ESC_NORMAL);
    printf("-----------\n");
    printf("For users who are not in the sudoers file, permission has been denied.\n");
    printf("This incident will be reported.\n");
    printf("\n");

    printf(ESC_BOLD "SCOPE AND LIMITATIONS\n" ESC_NORMAL);
    printf("---------------------\n");
    printf("This policy supersedes any applicable federal, state and local laws,\n");
    printf("regulations and ordinances, international treaties, and legal agreements that\n");
    printf("would otherwise apply. If any provision of this policy is found by a court to\n");
    printf("be unenforceable, it nevertheless remains in force.\n");
    printf("\n");

    printf("This organization is not liable and this agreement shall not be construed.\n");
    printf("These statements have not been peer reviewed. This badge is intended as a\n");
    printf("proof that P=NP.\n");
    printf("\n");
    return mp_const_none;
}
