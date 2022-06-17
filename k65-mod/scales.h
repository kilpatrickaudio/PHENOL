unsigned char scales[16][12] = {
// 5ths
{ 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7, 7 },
// 5ths and 7ths
{ 0, 0, 0, 0, 7, 7, 7, 7, 10, 10, 10, 10 },
// major
{ 0, 0, 0, 0, 4, 4, 4, 4, 7, 7, 7, 7 }, 
// major 7th down
{ 10, 10, 10, 7, 7, 7, 4, 4, 4, 0, 0, 0 },
// minor
{ 0, 0, 0, 0, 3, 3, 3, 3, 7, 7, 7, 7 },
// minor 7th
{ 0, 0, 0, 3, 3, 3, 7, 7, 7, 10, 10, 10 },
// minor 7th down
{ 10, 10, 10, 7, 7, 7, 3, 3, 3, 0, 0, 0 },
// minor alternating
{ 0, 0, 0, 0, 7, 7, 7, 7, 3, 3, 3, 3 },
// major alternating
{ 0, 0, 0, 0, 7, 7, 7, 7, 4, 4, 4, 4 },
// minor alternating 7ths
{ 0, 0, 0, 7, 7, 7, 3, 3, 3, 10, 10, 10 },
// major alternating 7ths
{ 0, 0, 0, 7, 7, 7, 4, 4, 4, 10, 10, 10 },
// clusters 1
{ 0, 0, 0, 2, 2, 2, 5, 5, 5, 7, 7, 7 },
// clusters 2
{ 0, 0, 0, 5, 5, 5, 7, 7, 7, 10, 10, 10 },
// jumping
{ 0, 0, 4, 4, 2, 2, 5, 5, 4, 4, 7, 7 },
// blues
{ 0, 0, 2, 2, 3, 3, 5, 5, 7, 7, 10, 10 },
// leading
{ 0, 0, 2, 2, 4, 4, 7, 7, 9, 9, 11, 11 }
};