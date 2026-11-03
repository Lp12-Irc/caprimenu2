using System;
using System.Globalization;

public static class KeyExpiry {
    public static bool TryRead(string value,out DateTimeOffset expiry) =>
        DateTimeOffset.TryParse(value,CultureInfo.InvariantCulture,DateTimeStyles.AssumeUniversal|DateTimeStyles.AdjustToUniversal,out expiry);
    public static bool IsExpired(string value,DateTimeOffset now) =>
        value!="never" && (!TryRead(value,out var expiry) || expiry<=now);
    public static string Remaining(string value,DateTimeOffset now) {
        if(value=="never")return "Sem vencimento";
        if(!TryRead(value,out var expiry))return "Data inválida";
        var left=expiry-now;
        if(left<=TimeSpan.Zero)return "Prazo encerrado";
        return left.TotalDays>=1?$"{(int)left.TotalDays}d {left.Hours:00}h":$"{(int)left.TotalHours:00}h {left.Minutes:00}m {left.Seconds:00}s";
    }
}
