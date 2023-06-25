import numpy as np

data = np.zeros((10, 10), dtype=int)

if 0:
    data[0, 0] = 1
    data[1, 0] = 1
    data[4, 5] = 1
    data[9, 9] = 1


def fill_grid_row(row_number, column_depth):
    """ Fill a row on the grid up to a depth of column_depth """
    for column_depth in range(column_depth):
        data[row_number, column_depth] = 1

def write_grid_row(row_number, column_depth):
    """Write to a row with a depth of column_depth """
    data[:column_depth, row_number] = 1
    data[column_depth:, row_number] = 0

# for index, value in np.ndenumerate(data):
#     row = index[0]
#     col = index[1]
#     print(f"Index: {index}. Value: {value}")

# fill_grid_row(1, 4)
# fill_grid_row(3, 9)
write_grid_row(1, 8)
print(data)

write_grid_row(1, 3)
print(data)