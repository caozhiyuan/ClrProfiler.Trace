using System;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Threading.Tasks;
using StackExchange.Redis;

namespace Samples.Redis
{
    class Program
    {
        static void Main(string[] args)
        {

            Console.WriteLine($"Is64BitProcess:{Environment.Is64BitProcess}");

            RunStackExchange("StackExchange").Wait();

            Console.ReadLine();
        }

        private static async Task RunStackExchange(string prefix)
        {
            prefix += "StackExchange.Redis.";

            Console.WriteLine($"Testing StackExchange.Redis {prefix}");
            using (var redis = ConnectionMultiplexer.Connect("localhost,allowAdmin=true"))
            {
                redis.Configure(Console.Out);

                var db = redis.GetDatabase(1);
                await db.StringSetAsync($"{prefix}INCR", "0");

                db.StringSet($"{prefix}INCR", "0");

                var c = db.StringGet($"{prefix}INCR");

                Console.WriteLine(c);
            }

            Console.ReadLine();
        }
    }
}
