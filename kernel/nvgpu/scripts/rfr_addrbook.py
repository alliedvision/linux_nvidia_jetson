# Copyright (c) 2019, NVIDIA CORPORATION.  All Rights Reserved.
#
# Nvgpu address book. Entries in here let us do translations from human readable
# email names/aliases to the real NV email addresses.
#
# This is structured as a dictionary with keys as human readable names which
# point to the real email address.
#
# It's fine to have multiple keys pointing to the same thing. Aliases are nice.
#

import re
import os

# The table itself is private. Use the methods below to do AB lookups, etc.
#
# Also please note: the table should be all lower case! This makes it easy to
# just lowercase all incoming queries so that we can do case insensitive
# lookups.
__rfr_address_book = { }

def __rfr_parse_addrbook(ab):
    """
    Parse an address book. Ignore empty lines and lines that begin with '#'.
    All other lines are split by the '|' character into 2 strings - a key and
    a value. The key is a nickname and the value is the real address. For
    example:

      alex | Alex Waterman <alexw@nvidia.com>

    Note: if there's a syntax error detected an error is always printed. Unlike
    if there's missing files in which errors may be silent.

    Returns True/False for pass/fail.
    """

    global __rfr_address_book

    tmp_ab = { }
    line_nr = 0

    for line in ab.readlines():
        line_nr += 1
        l = line.strip()

        if len(l) == 0 or l[0] == '#':
            continue

        # Only allow at most 1 split.
        kv = l.split('|', 1)

        if len(kv) != 2:
            print('Error: unable to parse address book. Invalid line:')
            print('  > \'%s\' @ line: %d' % (line, line_nr))
            return False

        tmp_ab[kv[0].strip().lower()] = kv[1].strip().lower()

    __rfr_address_book = tmp_ab
    return True

def __rfr_load_ab(path, silent=False):
    """
    Load the passed address book and print errors if silent is False.
    """

    success = True

    try:
        with open(path) as ab:
            if not __rfr_parse_addrbook(ab):
                success = False
    except Exception as err:
        success = False
        if not silent:
            print('Error: %s' % err)

    # It's not a very helpful error message I suppose. Eh. We will get more
    # detail from the __rfr_parse_addrbook() call itself.
    if not success and not silent:
        print('Failed to parse AB: %s' % path)

    return success

def rfr_ab_load(ab_path):
    """
    Attempt to load an address book.

    If ab_path is None then look for the book at the environment variable
    RFR_ADDRBOOK. If that's not found or fails to load then fall back to trying
    ~/.rfr-addrbook. If that doesn't work just silently return True and we will
    have an empty address book.

    If ab_path is not None then try to load the addr book from the passed path
    and if there's an error print an error message. This will return the result
    of the AB load.
    """

    if ab_path:
        return __rfr_load_ab(ab_path)

    # Ok, if we are here, do the whole env logic thing.
    ab_path = os.getenv('RFR_ADDRBOOK')
    if ab_path:
        success = __rfr_load_ab(ab_path, silent=True)
        if success:
            return True

    # Otherwise... Try ~/.rfr-addrbook
    ab_path = os.getenv('HOME') + '/.rfr-addrbook'
    if ab_path:
        success = __rfr_load_ab(ab_path, silent=True)
        if success:
            return True

    # Well, it all failed. But we don't care.
    return True

def rfr_ab_query(regex):
    """
    Print a list of keys that match the passed regex string.
    """

    matches = [ ]

    p = re.compile(regex, re.IGNORECASE)

    for k in __rfr_address_book.keys():
        m = p.search(k)
        if not m:
            continue

        matches.append(k)

    print('Address book query: %s' % regex)

    if not matches:
        print('> No matches')
        return

    for addr in sorted(matches):
        print('> %-15s | %s' % (addr, __rfr_address_book[addr]))

def rfr_ab_lookup_single(addr):
    """
    Exact lookup into the address book but if the addr isn't found in the keys
    then also check the values. If the addr is not found in the values then
    return None.

    This lets the rfr script either ignore the AB lookup failure or bail out.
    """

    # If there's no address book, just pass the addr through.
    if len(list(__rfr_address_book.keys())) == 0:
        return addr

    lc_addr = addr.lower()

    if lc_addr in __rfr_address_book:
        return __rfr_address_book[lc_addr]

    if lc_addr in list(__rfr_address_book.values()):
        # Return the orignal, un-lowercased.
        return addr

    return None

def rfr_ab_lookup(addrs, ignore_missing):
    """
    Take a list of addresses and look them up in the address book. Return a new
    list which contains the results of the lookups.

    If ignore_missing is True then if there's an address book lookup failure
    just ignore it and pass the address through to the new list. If False then
    fail and bail (return None).
    """

    lookups = [ ]

    for a in addrs:
        lookup = rfr_ab_lookup_single(a)
        if not lookup and not ignore_missing:
            print('Unknown address: %s' % a)
            return None

        if lookup:
            lookups.append(lookup)
        else:
            lookups.append(a)

    return lookups
