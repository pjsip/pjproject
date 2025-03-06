using libpjsua2.maui;

namespace pjsua2maui.Models;

public class SoftLogWriter : LogWriter
{
    override public void write(LogEntry entry)
    {
        Console.WriteLine(entry.msg);
    }
}