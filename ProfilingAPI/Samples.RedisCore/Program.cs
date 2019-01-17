using System;
using System.Diagnostics;
using System.Threading.Tasks;
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

            RunStackExchange("StackExchange").Wait();
        }

        private static string Host()
        {
            return Environment.GetEnvironmentVariable("REDIS_HOST") ?? "localhost";
        }

        private static async Task RunStackExchange(string prefix)
        {
            prefix += "StackExchange.Redis.";

            Console.WriteLine($"Testing StackExchange.Redis {prefix}");
            using (var redis = ConnectionMultiplexer.Connect(Host() + ",allowAdmin=true"))
            {
                redis.Configure(Console.Out);

                var db = redis.GetDatabase(1);
                var n = await db.StringSetAsync($"{prefix}INCR", "0");

                Stopwatch sw = new Stopwatch();
                sw.Start();
                for (int i = 0; i < 10000; i++)
                {
                    await db.StringSetAsync($"{prefix}INCR{i}", "0");
                }
                sw.Stop();
                Console.WriteLine(sw.ElapsedMilliseconds);
            }

            Console.ReadLine();
        }
    }
}
