using System;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Dapper;
using MySql.Data.MySqlClient;
using StackExchange.Redis;

namespace Samples.Console
{
    class Program
    {
        private static readonly HttpClient HttpClient = new HttpClient();

        static void Main(string[] args)
        {
            Run().GetAwaiter().GetResult();

            System.Console.ReadLine();
        }

        private static async Task Run()
        {
            System.Console.WriteLine("Start");

            Program program = new Program();

            await program.RunStackExchange();

            await program.RunGetBing();

            await program.RunSqlClient();

            await program.RunMySqlClient();

            System.Console.WriteLine("End");
        }

        private async Task RunGetBing()
        {
            await HttpClient.GetStringAsync("https://cn.bing.com/");
        }

        private async Task RunSqlClient()
        {
            using (var connection = new SqlConnection($"Data Source=.;Initial Catalog=tempdb;Integrated Security=True"))
            {
                await connection.OpenAsync();

                await connection.ExecuteScalarAsync("SELECT 1");

                await connection.ExecuteReaderAsync("SELECT 1");
            }
        }

        private async Task RunMySqlClient()
        {
            try
            {
                using (var connection = new MySqlConnection($"User ID=root;Password=123456;Host=127.0.0.1;Port=3306;Database=test;Pooling=true;Min Pool Size=0;Max Pool Size=100;SslMode=None"))
                {
                    connection.Open();

                    connection.ExecuteScalar("SELECT 1");

                    await connection.ExecuteReaderAsync("SELECT 1");
                }
            }
            catch (Exception e)
            {
                System.Console.WriteLine(e);
                throw;
            }
        }

        private async Task RunStackExchange()
        {
            var prefix = "StackExchange.Redis.";
            
            using (var redis = ConnectionMultiplexer.Connect("localhost,allowAdmin=true"))
            {
                var db = redis.GetDatabase(0);

                await db.StringSetAsync($"{prefix}INCR", "0");

                db.StringSet($"{prefix}INCR", DateTime.Now.ToLongDateString());

                db.StringGet($"{prefix}INCR");


                Stopwatch sw = new Stopwatch();
                sw.Start();
                const int c = 10000;
                CountdownEvent k = new CountdownEvent(c);
                Parallel.For(0, c, (i) =>
                {
                    var task = db.StringSetAsync($"{prefix}INCR{i}", "0");
                    task.ContinueWith(n =>
                    {
                        if (n.IsFaulted)
                        {
                            System.Console.WriteLine($"{i} {n.Exception}");
                        }
                        k.Signal(1);
                    });
                });
                k.Wait();
                System.Console.WriteLine("Redis StringSetAsync " + sw.ElapsedMilliseconds);
            }
        }
    }
}
