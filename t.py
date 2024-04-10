import random

def generate_decimal_numbers(lower, upper, count):
    decimal_numbers = []
    for _ in range(count):
        decimal_numbers.append(random.uniform(lower, upper))
    decimal_numbers.sort()
    return decimal_numbers

if __name__ == "__main__":
    lower_bound = 1
    upper_bound = 1_000_000_000
    numbers_count = 1_000_000_000
    
    sorted_decimal_numbers = generate_decimal_numbers(lower_bound, upper_bound, numbers_count)
    
    # Print first 10 sorted numbers for demonstration
  #  print("First 10 sorted decimal numbers:")
    for i in sorted_decimal_numbers: print(i)

