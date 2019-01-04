SECTIONS
{
  .testing_array  :
  {
    PROVIDE (__testing_array_start = .);
    KEEP (*(.testing_array))
    PROVIDE (__testing_array_end = .);
  }
}
INSERT AFTER .fini_array;
