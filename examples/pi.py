# ================================================================
# COMPUTING PI USING INTEGER ARITHMETIC — SPIGOT ALGORITHM
# ================================================================
#
# 1. THE MATHEMATICAL REPRESENTATION OF PI
# ---------------------------------------------------------------
#
# Pi has the following infinite representation:
#
#          1       2       3       4
# pi = 2 + - ( 2 + - ( 2 + - ( 2 + - ( ... ))))
#          3       5       7       9
#
# More formally, this can be written as a nested expression:
#
# pi = 2 + (1/3) * (
#          2 + (2/5) * (
#              2 + (3/7) * (
#                  2 + (4/9) * (...)
#              )
#          )
#      )
#
# Notice three important properties:
#
#   a. Every constant inside the expression is 2.
#
#   b. The successive multipliers are:
#
#          1/3, 2/5, 3/7, 4/9, ...
#
#      In general, the multiplier at position j is:
#
#          j / (2*j + 1)
#
#      when positions are counted starting from zero.
#
#   c. The expression converges to pi.
#
# This representation is particularly useful because its
# coefficients and numerators are all integers.
#
#
# 2. WHY MULTIPLYING BY 10 EXTRACTS DIGITS
# ---------------------------------------------------------------
#
# Consider an ordinary decimal number:
#
#     x = 3.14159265...
#
# We can separate its integer and fractional parts:
#
#     x = 3 + 0.14159265...
#
# Multiplying the fractional part by 10 gives:
#
#     10 * 0.14159265... = 1.4159265...
#
# The integer part is the next decimal digit: 1.
#
# Repeating:
#
#     10 * 0.4159265... = 4.159265...
#
# The next digit is 4.
#
# This is the basic principle of a spigot algorithm:
#
#     multiply by 10
#     extract the integer part
#     retain the fractional remainder
#     repeat
#
# The challenge is performing these operations without
# representing any fractional number.
#
#
# 3. REPRESENTING FRACTIONS AS INTEGER REMAINDERS
# ---------------------------------------------------------------
#
# Suppose we want to evaluate:
#
#     17 / 5
#
# Integer division gives:
#
#     quotient  = 17 // 5 = 3
#     remainder = 17 % 5  = 2
#
# Therefore:
#
#     17 / 5 = 3 + 2/5
#
# We have separated the integer portion from the remainder
# without performing floating-point division.
#
# The same idea works for the nested representation of pi.
#
# Instead of calculating the fractions directly, we keep
# their numerators as integer remainders.
#
# Every time we multiply by 10, we propagate those
# remainders through the nested expression.
#
#
# 4. THE ARRAY REPRESENTS THE NESTED EXPRESSION
# ---------------------------------------------------------------
#
# Initially, the representation contains only 2s:
#
#     a = [2, 2, 2, 2, 2, ...]
#
# Each element corresponds to one level of the nested
# expression:
#
#     a[0] + (1/3) * (
#         a[1] + (2/5) * (
#             a[2] + (3/7) * (...)
#         )
#     )
#
# Initially, every a[i] is 2.
#
# After extracting a digit, the elements no longer
# necessarily equal 2.
#
# They now store the integer remainders resulting from
# the previous extraction.
#
# In other words:
#
#     The array is the persistent fractional state of pi.
#
# It is NOT an array containing the decimal digits of pi.
#
#
# 5. PROPAGATING REMAINDERS FROM RIGHT TO LEFT
# ---------------------------------------------------------------
#
# Because the representation is nested, we process it
# starting from the innermost fraction.
#
# For each position j, we calculate:
#
#     x = 10 * a[j-1] + q * j
#
# Where:
#
#     a[j-1] = current integer remainder
#     q      = quotient propagated from the inner level
#     j      = numerator of the current fractional multiplier
#
# Why multiply by j?
#
# Because the nested representation contains the multiplier:
#
#     j / (2*j + 1)
#
# The algorithm uses one-based array positions, so the
# corresponding divisor is:
#
#     2*j - 1
#
# We now perform ordinary integer division:
#
#     a[j-1] = x % (2*j - 1)
#     q      = x // (2*j - 1)
#
# The remainder stays in the current array element.
# The quotient is propagated to the next outer level.
#
# This is the core of the algorithm.
#
# Each level converts its incoming value into:
#
#     quotient + remainder / divisor
#
# The quotient propagates outward.
# The remainder stays behind for the next iteration.
#
# Eventually, the quotient reaches the outermost level.
#
#
# 6. EXTRACTING A DECIMAL DIGIT
# ---------------------------------------------------------------
#
# After the entire array has been processed, q contains
# the scaled integer contribution from the representation.
#
# We separate it into:
#
#     a[0] = q % 10
#     q    = q // 10
#
# The modulo operation retains the decimal remainder.
#
# The integer division produces the candidate next digit.
#
# For example, if q is 31:
#
#     a[0] = 31 % 10 = 1
#     q    = 31 // 10 = 3
#
# We have obtained the first digit of pi: 3.
#
# The remaining state is preserved in the array.
#
#
# 7. WHY WE CANNOT ALWAYS OUTPUT A DIGIT IMMEDIATELY
# ---------------------------------------------------------------
#
# There is a subtle complication.
#
# The algorithm uses a finite approximation of an infinite
# representation. Consequently, a candidate digit may still
# be affected by a carry from a subsequent iteration.
#
# For example:
#
#     ...499999...
#
# might become:
#
#     ...500000...
#
# We therefore delay confirmation of certain digits.
#
# Three situations are possible:
#
# CASE A: q == 9
#
#     We cannot confirm the digit yet.
#     A future carry might turn it into zero.
#
#     Increment the number of pending nines.
#
# CASE B: q == 10
#
#     A carry has occurred.
#
#     Increment the previous digit and replace all
#     pending nines with zeros.
#
# CASE C: q < 9
#
#     No carry can propagate through this digit.
#
#     Confirm the previous digit and output any
#     pending nines.
#
# Two variables implement this:
#
#     predigit = most recent unconfirmed digit
#     nines    = number of pending nines
#
# This is called delayed carry propagation.
#
#
# 8. WHY THE ARRAY HAS APPROXIMATELY 10*n/3 ELEMENTS
# ---------------------------------------------------------------
#
# The infinite representation must be truncated because
# computers have finite memory.
#
# Fortunately, the contribution of deeper terms decreases
# rapidly.
#
# The truncation error decreases sufficiently quickly that
# approximately 10/3 terms per decimal digit provide enough
# precision for the classical algorithm.
#
# Thus:
#
#     array_size = n * 10 // 3 + 1
#
# For 100 digits, this gives approximately 334 elements.
#
# More digits require a longer array, but individual array
# entries remain small integers.
#
#
# 9. COMPUTATIONAL COMPLEXITY
# ---------------------------------------------------------------
#
# For n requested digits:
#
#     Array size:       O(n)
#     Work per digit:   O(n)
#     Total work:       O(n^2)
#     Memory usage:     O(n)
#
# The algorithm uses only:
#
#     addition
#     multiplication
#     integer division
#     modulo
#
# No floating-point numbers or mathematical libraries.
#
# ================================================================


def pi_digits(n):
    a = [2] * (n * 10 // 3 + 1)

    nines = 0
    predigit = 0

    for _ in range(n):
        q = 0

        # Propagate quotients from the innermost fraction
        # to the outermost fraction.
        for j in range(len(a), 0, -1):
            x = 10 * a[j - 1] + q * j

            a[j - 1] = x % (2 * j - 1)
            q = x // (2 * j - 1)

        # Extract candidate digit.
        a[0] = q % 10
        q //= 10

        # Delay potentially unsafe digits.
        if q == 9:
            nines += 1

        elif q == 10:
            yield predigit + 1

            for _ in range(nines):
                yield 0

            predigit = 0
            nines = 0

        else:
            yield predigit
            predigit = q

            for _ in range(nines):
                yield 9

            nines = 0

    yield predigit

    for _ in range(nines):
        yield 9


digits = list(pi_digits(30))
print("".join(str(d) for d in digits[1:]))
