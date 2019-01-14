using System;
using System.Diagnostics;
using StackExchange.Redis;

namespace Samples.RedisCore
{
    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                AppDomain.CurrentDomain.Load("Datadog.Trace.ClrProfiler.Managed");
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
            
            RunStackExchange("StackExchange");
        }

        private static string Host()
        {
            return Environment.GetEnvironmentVariable("REDIS_HOST") ?? "localhost";
        }

        private static void RunStackExchange(string prefix)
        {
            prefix += "StackExchange.Redis.";

            Console.WriteLine($"Testing StackExchange.Redis {prefix}");
            using (var redis = ConnectionMultiplexer.Connect(Host() + ",allowAdmin=true"))
            {
                redis.Configure(Console.Out);

                var db = redis.GetDatabase(1);
                db.StringSet($"{prefix}INCR", "0");

                Stopwatch sw = new Stopwatch();
                sw.Start();
                for (int i = 0; i < 30000; i++)
                {
                    db.StringSet($"{prefix}INCR{i}", "0");
                }
                sw.Stop();
                Console.WriteLine(sw.ElapsedMilliseconds);
            }

            Console.ReadLine();
        }
    }
}
