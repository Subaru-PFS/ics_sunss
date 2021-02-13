/* just turns all the pins off */
extern void alloff();
extern void motorsetup();

int
main()
{
    motorsetup();
    alloff();
    return 0 ;
}
