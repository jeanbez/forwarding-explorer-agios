/* File:	access_pattern_detection_tree.c
 * Created: 	2014
 * License:	GPL version 3
 * Author:
 *		Francieli Zanon Boito <francielizanon (at) gmail.com>
 *
 * Description:
 *		This file is part of the AGIOS I/O Scheduling tool.
 *		It provides a decision tree that detects access pattern aspects from 
 *		requests streams. It requires the "average distance between consecutive 
 *		requests" and "average stripe access time difference" metrics.
 *
 * Obs.: this file was generated automatically from the tree provided by the
 * Weka data mining tool.
 *
 * Contributors:
 *		Federal University of Rio Grande do Sul (UFRGS)
 *		INRIA France
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "agios.h"
#include "agios_request.h"
#include "access_pattern_detection_tree.h"

//timediff must come im ms
void access_pattern_detection_tree (int operation, long double avgdist, long double timediff, short int *spatiality, short int *reqsize)
{
	if (timediff < 0.975)
	{
		if (avgdist < 0.5)
		{
			if (operation == RT_READ)
			{
				if (timediff < 0.565)
				{
					*spatiality = AP_NONCONTIG;
					*reqsize = AP_SMALL;
					return;
				}
				if (timediff >= 0.565)
				{
					*spatiality = AP_CONTIG;
					*reqsize = AP_LARGE;
					return;
				}
			}
			if (operation != RT_READ)
			{
				if (timediff < 0.41000000000000003)
				{
					*spatiality = AP_CONTIG;
					*reqsize = AP_LARGE;
					return;
				}
				if (timediff >= 0.41000000000000003)
				{
					*spatiality = AP_NONCONTIG;
					*reqsize = AP_SMALL;
					return;
				}
			}
		}
		if (avgdist >= 0.5)
		{
			if (avgdist < 351.5)
			{
				if (timediff < 0.87)
				{
					if (timediff < 0.21000000000000002)
					{
						*spatiality = AP_NONCONTIG;
						*reqsize = AP_SMALL;
						return;
					}
					if (timediff >= 0.21000000000000002)
					{
						*spatiality = AP_NONCONTIG;
						*reqsize = AP_LARGE;
						return;
					}
				}
				if (timediff >= 0.87)
				{
					*spatiality = AP_NONCONTIG;
					*reqsize = AP_SMALL;
					return;
				}
			}
			if (avgdist >= 351.5)
			{
				*spatiality = AP_CONTIG;
				*reqsize = AP_LARGE;
				return;
			}
		}
	}
	if (timediff >= 0.975)
	{
		if (timediff < 8.195)
		{
			if (avgdist < 0.5)
			{
				if (timediff < 3.455)
				{
					if (timediff < 2.085)
					{
						if (timediff < 1.4049999999999998)
						{
							if (timediff < 1.025)
							{
								if (operation == RT_READ)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
								if (operation != RT_READ)
								{
									*spatiality = AP_NONCONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
							if (timediff >= 1.025)
							{
								if (timediff < 1.165)
								{
									if (timediff < 1.0550000000000002)
									{
										if (operation == RT_READ)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (operation != RT_READ)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (timediff >= 1.0550000000000002)
									{
										if (timediff < 1.145)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 1.145)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
								if (timediff >= 1.165)
								{
									if (timediff < 1.3450000000000002)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (timediff >= 1.3450000000000002)
									{
										if (timediff < 1.3650000000000002)
										{
											if (operation == RT_READ)
											{
												*spatiality = AP_NONCONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (operation != RT_READ)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
										if (timediff >= 1.3650000000000002)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
							}
						}
						if (timediff >= 1.4049999999999998)
						{
							if (operation == RT_READ)
							{
								if (timediff < 1.705)
								{
									if (timediff < 1.6349999999999998)
									{
										if (timediff < 1.525)
										{
											if (timediff < 1.495)
											{
												if (timediff < 1.455)
												{
													if (timediff < 1.4249999999999998)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 1.4249999999999998)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 1.455)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (timediff >= 1.495)
											{
												if (timediff < 1.505)
												{
													*spatiality = AP_NONCONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 1.505)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
										}
										if (timediff >= 1.525)
										{
											if (timediff < 1.585)
											{
												if (timediff < 1.5750000000000002)
												{
													if (timediff < 1.5350000000000001)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 1.5350000000000001)
													{
														if (timediff < 1.545)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 1.545)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
												if (timediff >= 1.5750000000000002)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (timediff >= 1.585)
											{
												if (timediff < 1.605)
												{
													if (timediff < 1.5950000000000002)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 1.5950000000000002)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 1.605)
												{
													if (timediff < 1.625)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 1.625)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
										}
									}
									if (timediff >= 1.6349999999999998)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
								if (timediff >= 1.705)
								{
									if (timediff < 1.745)
									{
										if (timediff < 1.725)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 1.725)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (timediff >= 1.745)
									{
										if (timediff < 1.8050000000000002)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 1.8050000000000002)
										{
											if (timediff < 1.855)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (timediff >= 1.855)
											{
												if (timediff < 1.915)
												{
													if (timediff < 1.9049999999999998)
													{
														if (timediff < 1.8849999999999998)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 1.8849999999999998)
														{
															if (timediff < 1.895)
															{
																*spatiality = AP_NONCONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 1.895)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
													}
													if (timediff >= 1.9049999999999998)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 1.915)
												{
													if (timediff < 1.9449999999999998)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 1.9449999999999998)
													{
														if (timediff < 1.9649999999999999)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 1.9649999999999999)
														{
															if (timediff < 1.995)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 1.995)
															{
																if (timediff < 2.0149999999999997)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 2.0149999999999997)
																{
																	if (timediff < 2.035)
																	{
																		*spatiality = AP_NONCONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 2.035)
																	{
																		if (timediff < 2.075)
																		{
																			if (timediff < 2.0549999999999997)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 2.0549999999999997)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 2.075)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
							if (operation != RT_READ)
							{
								*spatiality = AP_CONTIG;
								*reqsize = AP_SMALL;
								return;
							}
						}
					}
					if (timediff >= 2.085)
					{
						*spatiality = AP_CONTIG;
						*reqsize = AP_SMALL;
						return;
					}
				}
				if (timediff >= 3.455)
				{
					if (operation == RT_READ)
					{
						if (timediff < 4.205)
						{
							if (timediff < 3.615)
							{
								if (timediff < 3.495)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
								if (timediff >= 3.495)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
							if (timediff >= 3.615)
							{
								if (timediff < 3.955)
								{
									if (timediff < 3.915)
									{
										if (timediff < 3.6550000000000002)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 3.6550000000000002)
										{
											if (timediff < 3.685)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (timediff >= 3.685)
											{
												if (timediff < 3.755)
												{
													if (timediff < 3.745)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 3.745)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 3.755)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
									}
									if (timediff >= 3.915)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
								if (timediff >= 3.955)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
						}
						if (timediff >= 4.205)
						{
							if (timediff < 6.525)
							{
								if (timediff < 4.735)
								{
									if (timediff < 4.375)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (timediff >= 4.375)
									{
										if (timediff < 4.625)
										{
											if (timediff < 4.415)
											{
												if (timediff < 4.395)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 4.395)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 4.415)
											{
												if (timediff < 4.525)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 4.525)
												{
													if (timediff < 4.575)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 4.575)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
										}
										if (timediff >= 4.625)
										{
											if (timediff < 4.655)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (timediff >= 4.655)
											{
												if (timediff < 4.665)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 4.665)
												{
													if (timediff < 4.715)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 4.715)
													{
														if (timediff < 4.725)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 4.725)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
										}
									}
								}
								if (timediff >= 4.735)
								{
									if (timediff < 5.785)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (timediff >= 5.785)
									{
										if (timediff < 6.395)
										{
											if (timediff < 6.205)
											{
												if (timediff < 6.025)
												{
													if (timediff < 5.975)
													{
														if (timediff < 5.905)
														{
															if (timediff < 5.865)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 5.865)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (timediff >= 5.905)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 5.975)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 6.025)
												{
													if (timediff < 6.115)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 6.115)
													{
														if (timediff < 6.145)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 6.145)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
											if (timediff >= 6.205)
											{
												if (timediff < 6.235)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 6.235)
												{
													if (timediff < 6.345)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 6.345)
													{
														if (timediff < 6.365)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 6.365)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
										}
										if (timediff >= 6.395)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
								}
							}
							if (timediff >= 6.525)
							{
								if (timediff < 8.145)
								{
									if (timediff < 6.585)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (timediff >= 6.585)
									{
										if (timediff < 7.91)
										{
											if (timediff < 7.085)
											{
												if (timediff < 7.015)
												{
													if (timediff < 6.955)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 6.955)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 7.015)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 7.085)
											{
												if (timediff < 7.095)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 7.095)
												{
													if (timediff < 7.195)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 7.195)
													{
														if (timediff < 7.265)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 7.265)
														{
															if (timediff < 7.285)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 7.285)
															{
																if (timediff < 7.345)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 7.345)
																{
																	if (timediff < 7.885)
																	{
																		if (timediff < 7.745)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 7.745)
																		{
																			if (timediff < 7.835)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 7.835)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																	}
																	if (timediff >= 7.885)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
															}
														}
													}
												}
											}
										}
										if (timediff >= 7.91)
										{
											if (timediff < 7.9350000000000005)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (timediff >= 7.9350000000000005)
											{
												if (timediff < 8.094999999999999)
												{
													if (timediff < 7.955)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 7.955)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 8.094999999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
									}
								}
								if (timediff >= 8.145)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
						}
					}
					if (operation != RT_READ)
					{
						if (timediff < 5.635)
						{
							*spatiality = AP_CONTIG;
							*reqsize = AP_SMALL;
							return;
						}
						if (timediff >= 5.635)
						{
							if (timediff < 6.965)
							{
								if (timediff < 5.985)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
								if (timediff >= 5.985)
								{
									if (timediff < 6.705)
									{
										if (timediff < 6.555)
										{
											if (timediff < 6.455)
											{
												if (timediff < 6.425)
												{
													if (timediff < 6.275)
													{
														if (timediff < 6.265)
														{
															if (timediff < 6.175)
															{
																if (timediff < 6.105)
																{
																	if (timediff < 6.075)
																	{
																		if (timediff < 6.0649999999999995)
																		{
																			if (timediff < 5.995)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 5.995)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 6.0649999999999995)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 6.075)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 6.105)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 6.175)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (timediff >= 6.265)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 6.275)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 6.425)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 6.455)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
										if (timediff >= 6.555)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
									if (timediff >= 6.705)
									{
										if (timediff < 6.745)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 6.745)
										{
											if (timediff < 6.925)
											{
												if (timediff < 6.915)
												{
													if (timediff < 6.865)
													{
														if (timediff < 6.835)
														{
															if (timediff < 6.825)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 6.825)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 6.835)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 6.865)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 6.915)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (timediff >= 6.925)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
							if (timediff >= 6.965)
							{
								*spatiality = AP_CONTIG;
								*reqsize = AP_SMALL;
								return;
							}
						}
					}
				}
			}
			if (avgdist >= 0.5)
			{
				if (avgdist < 1319.0)
				{
					if (avgdist < 14.5)
					{
						if (avgdist < 8.0)
						{
							*spatiality = AP_NONCONTIG;
							*reqsize = AP_SMALL;
							return;
						}
						if (avgdist >= 8.0)
						{
							if (timediff < 5.359999999999999)
							{
								*spatiality = AP_NONCONTIG;
								*reqsize = AP_LARGE;
								return;
							}
							if (timediff >= 5.359999999999999)
							{
								*spatiality = AP_NONCONTIG;
								*reqsize = AP_SMALL;
								return;
							}
						}
					}
					if (avgdist >= 14.5)
					{
						*spatiality = AP_NONCONTIG;
						*reqsize = AP_LARGE;
						return;
					}
				}
				if (avgdist >= 1319.0)
				{
					if (avgdist < 1899.5)
					{
						if (timediff < 1.375)
						{
							*spatiality = AP_CONTIG;
							*reqsize = AP_LARGE;
							return;
						}
						if (timediff >= 1.375)
						{
							if (timediff < 1.615)
							{
								if (avgdist < 1691.0)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (avgdist >= 1691.0)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
							if (timediff >= 1.615)
							{
								if (timediff < 3.84)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (timediff >= 3.84)
								{
									if (avgdist < 1649.5)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (avgdist >= 1649.5)
									{
										if (avgdist < 1764.5)
										{
											if (timediff < 7.51)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 7.51)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
										if (avgdist >= 1764.5)
										{
											if (operation == RT_READ)
											{
												if (avgdist < 1779.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 1779.5)
												{
													if (timediff < 7.4)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 7.4)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
											if (operation != RT_READ)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
						}
					}
					if (avgdist >= 1899.5)
					{
						if (avgdist < 4206.5)
						{
							if (avgdist < 2199.5)
							{
								if (timediff < 2.1100000000000003)
								{
									if (timediff < 1.56)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (timediff >= 1.56)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
								if (timediff >= 2.1100000000000003)
								{
									if (timediff < 5.34)
									{
										if (timediff < 3.5149999999999997)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 3.5149999999999997)
										{
											if (avgdist < 2017.0)
											{
												if (timediff < 5.14)
												{
													if (timediff < 4.105)
													{
														if (timediff < 3.62)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 3.62)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 4.105)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 5.14)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (avgdist >= 2017.0)
											{
												if (timediff < 4.460000000000001)
												{
													if (timediff < 4.33)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 4.33)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 4.460000000000001)
												{
													if (operation == RT_READ)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (operation != RT_READ)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
										}
									}
									if (timediff >= 5.34)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
							}
							if (avgdist >= 2199.5)
							{
								if (avgdist < 3106.5)
								{
									if (avgdist < 2284.5)
									{
										if (operation == RT_READ)
										{
											if (timediff < 3.16)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 3.16)
											{
												if (timediff < 5.914999999999999)
												{
													if (timediff < 5.515000000000001)
													{
														if (avgdist < 2271.5)
														{
															if (timediff < 3.265)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 3.265)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 2271.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 5.515000000000001)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 5.914999999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
										if (operation != RT_READ)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (avgdist >= 2284.5)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
								if (avgdist >= 3106.5)
								{
									if (timediff < 4.279999999999999)
									{
										if (timediff < 2.225)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 2.225)
										{
											if (avgdist < 3222.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (avgdist >= 3222.0)
											{
												if (operation == RT_READ)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (operation != RT_READ)
												{
													if (avgdist < 3589.5)
													{
														if (avgdist < 3465.0)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (avgdist >= 3465.0)
														{
															if (avgdist < 3557.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 3557.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
													if (avgdist >= 3589.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
										}
									}
									if (timediff >= 4.279999999999999)
									{
										if (operation == RT_READ)
										{
											if (avgdist < 3721.5)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (avgdist >= 3721.5)
											{
												if (avgdist < 4031.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 4031.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
										if (operation != RT_READ)
										{
											if (avgdist < 3184.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (avgdist >= 3184.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
						}
						if (avgdist >= 4206.5)
						{
							if (avgdist < 4685.5)
							{
								if (avgdist < 4288.0)
								{
									if (timediff < 5.06)
									{
										if (timediff < 2.745)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 2.745)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (timediff >= 5.06)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
								if (avgdist >= 4288.0)
								{
									if (timediff < 3.965)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (timediff >= 3.965)
									{
										if (timediff < 4.29)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 4.29)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
								}
							}
							if (avgdist >= 4685.5)
							{
								if (timediff < 4.41)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (timediff >= 4.41)
								{
									if (avgdist < 5015.5)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (avgdist >= 5015.5)
									{
										if (operation == RT_READ)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (operation != RT_READ)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
							}
						}
					}
				}
			}
		}
		if (timediff >= 8.195)
		{
			if (avgdist < 124.5)
			{
				if (avgdist < 74.5)
				{
					if (timediff < 4625.78)
					{
						if (operation == RT_READ)
						{
							if (avgdist < 7.5)
							{
								if (timediff < 13.975000000000001)
								{
									if (timediff < 11.035)
									{
										if (avgdist < 0.5)
										{
											if (timediff < 10.385000000000002)
											{
												if (timediff < 9.955)
												{
													if (timediff < 9.535)
													{
														if (timediff < 9.495000000000001)
														{
															if (timediff < 9.405000000000001)
															{
																if (timediff < 9.385000000000002)
																{
																	if (timediff < 9.355)
																	{
																		if (timediff < 9.14)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 9.14)
																		{
																			if (timediff < 9.205)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 9.205)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																	}
																	if (timediff >= 9.355)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 9.385000000000002)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 9.405000000000001)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 9.495000000000001)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 9.535)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 9.955)
												{
													if (timediff < 10.094999999999999)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 10.094999999999999)
													{
														if (timediff < 10.235)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 10.235)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
											}
											if (timediff >= 10.385000000000002)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
										if (avgdist >= 0.5)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (timediff >= 11.035)
									{
										if (avgdist < 0.5)
										{
											if (timediff < 12.52)
											{
												if (timediff < 12.475000000000001)
												{
													if (timediff < 11.06)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 11.06)
													{
														if (timediff < 12.295)
														{
															if (timediff < 11.274999999999999)
															{
																if (timediff < 11.215)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 11.215)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 11.274999999999999)
															{
																if (timediff < 11.295)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 11.295)
																{
																	if (timediff < 11.305)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 11.305)
																	{
																		if (timediff < 11.435)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 11.435)
																		{
																			if (timediff < 11.465)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 11.465)
																			{
																				if (timediff < 11.49)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 11.49)
																				{
																					if (timediff < 11.594999999999999)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																					if (timediff >= 11.594999999999999)
																					{
																						if (timediff < 11.675)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 11.675)
																						{
																							if (timediff < 11.7)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																							if (timediff >= 11.7)
																							{
																								if (timediff < 11.785)
																								{
																									if (timediff < 11.754999999999999)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																									if (timediff >= 11.754999999999999)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																								}
																								if (timediff >= 11.785)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
														if (timediff >= 12.295)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
												if (timediff >= 12.475000000000001)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 12.52)
											{
												if (timediff < 13.614999999999998)
												{
													if (timediff < 13.434999999999999)
													{
														if (timediff < 13.405000000000001)
														{
															if (timediff < 12.545)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 12.545)
															{
																if (timediff < 13.195)
																{
																	if (timediff < 13.105)
																	{
																		if (timediff < 12.605)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 12.605)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 13.105)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 13.195)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
														if (timediff >= 13.405000000000001)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 13.434999999999999)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 13.614999999999998)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
										}
										if (avgdist >= 0.5)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
								if (timediff >= 13.975000000000001)
								{
									if (avgdist < 2.5)
									{
										if (timediff < 18.265)
										{
											if (timediff < 14.175)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 14.175)
											{
												if (timediff < 15.184999999999999)
												{
													if (timediff < 15.015)
													{
														if (timediff < 14.375)
														{
															if (timediff < 14.305)
															{
																if (timediff < 14.215)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 14.215)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 14.305)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 14.375)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 15.015)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 15.184999999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
										if (timediff >= 18.265)
										{
											if (timediff < 399.28)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 399.28)
											{
												*spatiality = AP_NONCONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
									}
									if (avgdist >= 2.5)
									{
										if (timediff < 369.1)
										{
											if (avgdist < 3.5)
											{
												if (timediff < 288.54999999999995)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 288.54999999999995)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (avgdist >= 3.5)
											{
												if (timediff < 293.26)
												{
													if (timediff < 129.11)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 129.11)
													{
														if (timediff < 287.385)
														{
															if (timediff < 264.86)
															{
																if (timediff < 247.76)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 247.76)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 264.86)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 287.385)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
												if (timediff >= 293.26)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
										}
										if (timediff >= 369.1)
										{
											if (timediff < 1332.855)
											{
												if (timediff < 566.665)
												{
													if (avgdist < 5.5)
													{
														if (timediff < 432.83500000000004)
														{
															if (avgdist < 4.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 4.5)
															{
																if (timediff < 419.0)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 419.0)
																{
																	if (timediff < 429.16)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 429.16)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
														}
														if (timediff >= 432.83500000000004)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (avgdist >= 5.5)
													{
														if (timediff < 494.495)
														{
															if (avgdist < 6.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (avgdist >= 6.5)
															{
																if (timediff < 485.905)
																{
																	if (timediff < 431.64)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 431.64)
																	{
																		if (timediff < 469.74)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 469.74)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
																if (timediff >= 485.905)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
														}
														if (timediff >= 494.495)
														{
															if (avgdist < 6.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 6.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
												if (timediff >= 566.665)
												{
													if (timediff < 1112.05)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 1112.05)
													{
														if (timediff < 1172.6950000000002)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 1172.6950000000002)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
											}
											if (timediff >= 1332.855)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
							if (avgdist >= 7.5)
							{
								if (timediff < 425.68)
								{
									if (timediff < 13.175)
									{
										*spatiality = AP_NONCONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (timediff >= 13.175)
									{
										*spatiality = AP_NONCONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
								if (timediff >= 425.68)
								{
									if (timediff < 3329.4350000000004)
									{
										if (timediff < 1177.25)
										{
											if (avgdist < 11.5)
											{
												if (timediff < 888.585)
												{
													if (avgdist < 8.5)
													{
														if (timediff < 677.165)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 677.165)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (avgdist >= 8.5)
													{
														if (timediff < 851.44)
														{
															if (timediff < 528.9649999999999)
															{
																if (timediff < 512.255)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 512.255)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 528.9649999999999)
															{
																if (timediff < 587.19)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 587.19)
																{
																	if (timediff < 606.6700000000001)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 606.6700000000001)
																	{
																		if (timediff < 782.425)
																		{
																			if (timediff < 625.145)
																			{
																				if (timediff < 612.3199999999999)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 612.3199999999999)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 625.145)
																			{
																				if (timediff < 644.4649999999999)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 644.4649999999999)
																				{
																					if (timediff < 657.505)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																					if (timediff >= 657.505)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																			}
																		}
																		if (timediff >= 782.425)
																		{
																			if (avgdist < 9.5)
																			{
																				if (timediff < 799.915)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (timediff >= 799.915)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (avgdist >= 9.5)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																	}
																}
															}
														}
														if (timediff >= 851.44)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
												if (timediff >= 888.585)
												{
													if (timediff < 1163.67)
													{
														if (avgdist < 8.5)
														{
															if (timediff < 957.505)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 957.505)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 8.5)
														{
															if (timediff < 905.865)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 905.865)
															{
																if (timediff < 1149.21)
																{
																	if (timediff < 1111.375)
																	{
																		if (timediff < 1018.4549999999999)
																		{
																			if (timediff < 984.9100000000001)
																			{
																				if (timediff < 976.61)
																				{
																					if (timediff < 940.78)
																					{
																						if (timediff < 916.155)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 916.155)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 940.78)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																				if (timediff >= 976.61)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 984.9100000000001)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																		if (timediff >= 1018.4549999999999)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 1111.375)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 1149.21)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
													}
													if (timediff >= 1163.67)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
											if (avgdist >= 11.5)
											{
												if (timediff < 636.9449999999999)
												{
													if (avgdist < 36.0)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (avgdist >= 36.0)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 636.9449999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
										}
										if (timediff >= 1177.25)
										{
											if (timediff < 2085.205)
											{
												if (avgdist < 14.5)
												{
													if (timediff < 1875.59)
													{
														if (avgdist < 12.5)
														{
															if (timediff < 1761.08)
															{
																if (avgdist < 8.5)
																{
																	if (timediff < 1502.345)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 1502.345)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (avgdist >= 8.5)
																{
																	if (timediff < 1253.76)
																	{
																		if (timediff < 1225.855)
																		{
																			if (timediff < 1190.42)
																			{
																				if (avgdist < 11.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (avgdist >= 11.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 1190.42)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 1225.855)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 1253.76)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
															}
															if (timediff >= 1761.08)
															{
																if (avgdist < 9.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (avgdist >= 9.5)
																{
																	if (timediff < 1770.06)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 1770.06)
																	{
																		if (timediff < 1781.775)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 1781.775)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
															}
														}
														if (avgdist >= 12.5)
														{
															if (timediff < 1648.365)
															{
																if (timediff < 1439.2849999999999)
																{
																	if (timediff < 1202.495)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 1202.495)
																	{
																		if (timediff < 1363.805)
																		{
																			if (timediff < 1213.4850000000001)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 1213.4850000000001)
																			{
																				if (timediff < 1238.5700000000002)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (timediff >= 1238.5700000000002)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																		}
																		if (timediff >= 1363.805)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
																if (timediff >= 1439.2849999999999)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 1648.365)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
													}
													if (timediff >= 1875.59)
													{
														if (avgdist < 11.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (avgdist >= 11.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
												if (avgdist >= 14.5)
												{
													if (timediff < 1863.31)
													{
														if (avgdist < 21.5)
														{
															if (timediff < 1322.79)
															{
																if (timediff < 1242.335)
																{
																	if (timediff < 1181.045)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 1181.045)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 1242.335)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 1322.79)
															{
																if (timediff < 1600.505)
																{
																	if (avgdist < 15.5)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (avgdist >= 15.5)
																	{
																		if (timediff < 1527.975)
																		{
																			if (timediff < 1498.37)
																			{
																				if (timediff < 1430.4850000000001)
																				{
																					if (timediff < 1419.125)
																					{
																						if (avgdist < 16.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (avgdist >= 16.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 1419.125)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																				if (timediff >= 1430.4850000000001)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 1498.37)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																		if (timediff >= 1527.975)
																		{
																			if (timediff < 1581.74)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 1581.74)
																			{
																				if (timediff < 1593.745)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 1593.745)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																		}
																	}
																}
																if (timediff >= 1600.505)
																{
																	if (timediff < 1648.955)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 1648.955)
																	{
																		if (timediff < 1733.155)
																		{
																			if (avgdist < 18.5)
																			{
																				if (timediff < 1671.805)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 1671.805)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (avgdist >= 18.5)
																			{
																				if (timediff < 1658.16)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (timediff >= 1658.16)
																				{
																					if (timediff < 1720.1799999999998)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																					if (timediff >= 1720.1799999999998)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																			}
																		}
																		if (timediff >= 1733.155)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																}
															}
														}
														if (avgdist >= 21.5)
														{
															if (timediff < 1813.935)
															{
																if (timediff < 1663.3)
																{
																	if (avgdist < 25.5)
																	{
																		if (timediff < 1387.37)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 1387.37)
																		{
																			if (timediff < 1408.935)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 1408.935)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																	}
																	if (avgdist >= 25.5)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 1663.3)
																{
																	if (timediff < 1709.105)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 1709.105)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
															if (timediff >= 1813.935)
															{
																if (timediff < 1829.8449999999998)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 1829.8449999999998)
																{
																	if (timediff < 1852.905)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 1852.905)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
															}
														}
													}
													if (timediff >= 1863.31)
													{
														if (avgdist < 30.5)
														{
															if (timediff < 2060.2749999999996)
															{
																if (timediff < 2050.025)
																{
																	if (avgdist < 19.5)
																	{
																		if (avgdist < 15.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (avgdist >= 15.5)
																		{
																			if (timediff < 1945.24)
																			{
																				if (timediff < 1897.99)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 1897.99)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 1945.24)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																	}
																	if (avgdist >= 19.5)
																	{
																		if (avgdist < 24.5)
																		{
																			if (timediff < 2004.44)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 2004.44)
																			{
																				if (timediff < 2014.69)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 2014.69)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																		}
																		if (avgdist >= 24.5)
																		{
																			if (timediff < 2011.53)
																			{
																				if (timediff < 1893.835)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (timediff >= 1893.835)
																				{
																					if (timediff < 2005.94)
																					{
																						if (avgdist < 27.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																						if (avgdist >= 27.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																					if (timediff >= 2005.94)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																			}
																			if (timediff >= 2011.53)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																	}
																}
																if (timediff >= 2050.025)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 2060.2749999999996)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 30.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
											if (timediff >= 2085.205)
											{
												if (avgdist < 28.5)
												{
													if (avgdist < 15.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 15.5)
													{
														if (timediff < 2839.35)
														{
															if (avgdist < 24.5)
															{
																if (timediff < 2679.235)
																{
																	if (timediff < 2466.355)
																	{
																		if (timediff < 2130.725)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 2130.725)
																		{
																			if (avgdist < 23.5)
																			{
																				if (avgdist < 20.5)
																				{
																					if (timediff < 2451.285)
																					{
																						if (timediff < 2205.13)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 2205.13)
																						{
																							if (timediff < 2326.73)
																							{
																								if (timediff < 2255.585)
																								{
																									if (timediff < 2222.8900000000003)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																									if (timediff >= 2222.8900000000003)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																								}
																								if (timediff >= 2255.585)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_SMALL;
																									return;
																								}
																							}
																							if (timediff >= 2326.73)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																						}
																					}
																					if (timediff >= 2451.285)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																				if (avgdist >= 20.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (avgdist >= 23.5)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																	}
																	if (timediff >= 2466.355)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 2679.235)
																{
																	if (avgdist < 18.5)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (avgdist >= 18.5)
																	{
																		if (avgdist < 22.5)
																		{
																			if (timediff < 2773.0550000000003)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 2773.0550000000003)
																			{
																				if (timediff < 2805.525)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 2805.525)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																		}
																		if (avgdist >= 22.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																}
															}
															if (avgdist >= 24.5)
															{
																if (timediff < 2595.145)
																{
																	if (timediff < 2498.4399999999996)
																	{
																		if (timediff < 2451.8)
																		{
																			if (timediff < 2391.855)
																			{
																				if (timediff < 2361.925)
																				{
																					if (timediff < 2131.44)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																					if (timediff >= 2131.44)
																					{
																						if (timediff < 2195.68)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																						if (timediff >= 2195.68)
																						{
																							if (timediff < 2216.725)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																							if (timediff >= 2216.725)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																						}
																					}
																				}
																				if (timediff >= 2361.925)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 2391.855)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 2451.8)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 2498.4399999999996)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 2595.145)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
														}
														if (timediff >= 2839.35)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
												if (avgdist >= 28.5)
												{
													if (avgdist < 42.5)
													{
														if (timediff < 2394.1400000000003)
														{
															if (avgdist < 34.5)
															{
																if (timediff < 2356.465)
																{
																	if (timediff < 2133.1800000000003)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 2133.1800000000003)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 2356.465)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (avgdist >= 34.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 2394.1400000000003)
														{
															if (avgdist < 30.5)
															{
																if (timediff < 2940.625)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 2940.625)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (avgdist >= 30.5)
															{
																if (timediff < 3105.755)
																{
																	if (timediff < 3066.76)
																	{
																		if (avgdist < 40.5)
																		{
																			if (timediff < 2667.43)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 2667.43)
																			{
																				if (timediff < 2699.4449999999997)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 2699.4449999999997)
																				{
																					if (avgdist < 32.5)
																					{
																						if (timediff < 2976.135)
																						{
																							if (timediff < 2788.545)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																							if (timediff >= 2788.545)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																						}
																						if (timediff >= 2976.135)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																					if (avgdist >= 32.5)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																			}
																		}
																		if (avgdist >= 40.5)
																		{
																			if (timediff < 2630.615)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 2630.615)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																	}
																	if (timediff >= 3066.76)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 3105.755)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
													}
													if (avgdist >= 42.5)
													{
														if (timediff < 3258.51)
														{
															if (avgdist < 47.5)
															{
																if (timediff < 3156.54)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 3156.54)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (avgdist >= 47.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 3258.51)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
											}
										}
									}
									if (timediff >= 3329.4350000000004)
									{
										if (avgdist < 36.5)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (avgdist >= 36.5)
										{
											if (avgdist < 62.5)
											{
												if (timediff < 4293.445)
												{
													if (avgdist < 40.5)
													{
														if (timediff < 3643.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 3643.5)
														{
															if (timediff < 3983.46)
															{
																if (timediff < 3756.23)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 3756.23)
																{
																	if (timediff < 3924.4049999999997)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 3924.4049999999997)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
															if (timediff >= 3983.46)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
													if (avgdist >= 40.5)
													{
														if (timediff < 3688.74)
														{
															if (avgdist < 50.5)
															{
																if (timediff < 3637.27)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 3637.27)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (avgdist >= 50.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 3688.74)
														{
															if (avgdist < 61.0)
															{
																if (timediff < 4024.355)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 4024.355)
																{
																	if (timediff < 4052.92)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 4052.92)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
															}
															if (avgdist >= 61.0)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
												if (timediff >= 4293.445)
												{
													if (avgdist < 45.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 45.5)
													{
														if (avgdist < 53.0)
														{
															if (timediff < 4599.66)
															{
																if (timediff < 4572.055)
																{
																	if (timediff < 4463.135)
																	{
																		if (timediff < 4410.105)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 4410.105)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 4463.135)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 4572.055)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 4599.66)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 53.0)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
											}
											if (avgdist >= 62.5)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
						}
						if (operation != RT_READ)
						{
							if (avgdist < 0.5)
							{
								if (timediff < 17.925)
								{
									if (timediff < 10.165)
									{
										if (timediff < 9.655000000000001)
										{
											if (timediff < 8.335)
											{
												if (timediff < 8.265)
												{
													if (timediff < 8.225000000000001)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 8.225000000000001)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 8.265)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 8.335)
											{
												if (timediff < 8.594999999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 8.594999999999999)
												{
													if (timediff < 8.684999999999999)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 8.684999999999999)
													{
														if (timediff < 8.71)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 8.71)
														{
															if (timediff < 8.805)
															{
																if (timediff < 8.785)
																{
																	if (timediff < 8.735)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 8.735)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 8.785)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 8.805)
															{
																if (timediff < 9.165)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 9.165)
																{
																	if (timediff < 9.265)
																	{
																		if (timediff < 9.225000000000001)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 9.225000000000001)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 9.265)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
															}
														}
													}
												}
											}
										}
										if (timediff >= 9.655000000000001)
										{
											if (timediff < 10.155000000000001)
											{
												if (timediff < 9.844999999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 9.844999999999999)
												{
													if (timediff < 9.885000000000002)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 9.885000000000002)
													{
														if (timediff < 9.934999999999999)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 9.934999999999999)
														{
															if (timediff < 9.995000000000001)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 9.995000000000001)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
											}
											if (timediff >= 10.155000000000001)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
									}
									if (timediff >= 10.165)
									{
										if (timediff < 14.285)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 14.285)
										{
											if (timediff < 17.255000000000003)
											{
												if (timediff < 14.305)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 14.305)
												{
													if (timediff < 14.725000000000001)
													{
														if (timediff < 14.594999999999999)
														{
															if (timediff < 14.455)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 14.455)
															{
																if (timediff < 14.585)
																{
																	if (timediff < 14.465)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 14.465)
																	{
																		if (timediff < 14.495000000000001)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 14.495000000000001)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
																if (timediff >= 14.585)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
														if (timediff >= 14.594999999999999)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 14.725000000000001)
													{
														if (timediff < 14.805)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 14.805)
														{
															if (timediff < 14.825)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 14.825)
															{
																if (timediff < 15.695)
																{
																	if (timediff < 15.665)
																	{
																		if (timediff < 15.655000000000001)
																		{
																			if (timediff < 15.515)
																			{
																				if (timediff < 15.445)
																				{
																					if (timediff < 15.355)
																					{
																						if (timediff < 15.315000000000001)
																						{
																							if (timediff < 15.245000000000001)
																							{
																								if (timediff < 15.225000000000001)
																								{
																									if (timediff < 15.205)
																									{
																										if (timediff < 15.195)
																										{
																											if (timediff < 15.155000000000001)
																											{
																												if (timediff < 15.135000000000002)
																												{
																													if (timediff < 15.105)
																													{
																														if (timediff < 15.065000000000001)
																														{
																															if (timediff < 15.015)
																															{
																																if (timediff < 14.934999999999999)
																																{
																																	*spatiality = AP_CONTIG;
																																	*reqsize = AP_LARGE;
																																	return;
																																}
																																if (timediff >= 14.934999999999999)
																																{
																																	*spatiality = AP_CONTIG;
																																	*reqsize = AP_SMALL;
																																	return;
																																}
																															}
																															if (timediff >= 15.015)
																															{
																																*spatiality = AP_CONTIG;
																																*reqsize = AP_LARGE;
																																return;
																															}
																														}
																														if (timediff >= 15.065000000000001)
																														{
																															*spatiality = AP_CONTIG;
																															*reqsize = AP_SMALL;
																															return;
																														}
																													}
																													if (timediff >= 15.105)
																													{
																														*spatiality = AP_CONTIG;
																														*reqsize = AP_LARGE;
																														return;
																													}
																												}
																												if (timediff >= 15.135000000000002)
																												{
																													*spatiality = AP_CONTIG;
																													*reqsize = AP_SMALL;
																													return;
																												}
																											}
																											if (timediff >= 15.155000000000001)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_LARGE;
																												return;
																											}
																										}
																										if (timediff >= 15.195)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																									}
																									if (timediff >= 15.205)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																								}
																								if (timediff >= 15.225000000000001)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_SMALL;
																									return;
																								}
																							}
																							if (timediff >= 15.245000000000001)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																						}
																						if (timediff >= 15.315000000000001)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 15.355)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																				if (timediff >= 15.445)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 15.515)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																		if (timediff >= 15.655000000000001)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 15.665)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 15.695)
																{
																	if (timediff < 16.675)
																	{
																		if (timediff < 16.549999999999997)
																		{
																			if (timediff < 16.515)
																			{
																				if (timediff < 16.494999999999997)
																				{
																					if (timediff < 16.445)
																					{
																						if (timediff < 16.335)
																						{
																							if (timediff < 16.295)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																							if (timediff >= 16.295)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																						}
																						if (timediff >= 16.335)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																					if (timediff >= 16.445)
																					{
																						if (timediff < 16.475)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																						if (timediff >= 16.475)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																				}
																				if (timediff >= 16.494999999999997)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 16.515)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 16.549999999999997)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 16.675)
																	{
																		if (timediff < 16.775)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 16.775)
																		{
																			if (timediff < 16.814999999999998)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 16.814999999999998)
																			{
																				if (timediff < 17.095)
																				{
																					if (timediff < 16.855)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																					if (timediff >= 16.855)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																				if (timediff >= 17.095)
																				{
																					if (timediff < 17.235)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																					if (timediff >= 17.235)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
											if (timediff >= 17.255000000000003)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
								if (timediff >= 17.925)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
							if (avgdist >= 0.5)
							{
								if (timediff < 264.9)
								{
									if (timediff < 34.575)
									{
										if (avgdist < 58.5)
										{
											if (avgdist < 14.5)
											{
												*spatiality = AP_NONCONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (avgdist >= 14.5)
											{
												if (avgdist < 15.5)
												{
													*spatiality = AP_NONCONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 15.5)
												{
													*spatiality = AP_NONCONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
										if (avgdist >= 58.5)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
									if (timediff >= 34.575)
									{
										*spatiality = AP_NONCONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
								if (timediff >= 264.9)
								{
									if (avgdist < 33.5)
									{
										if (timediff < 2633.565)
										{
											if (avgdist < 20.5)
											{
												if (timediff < 1526.705)
												{
													if (avgdist < 7.5)
													{
														if (timediff < 342.305)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 342.305)
														{
															if (timediff < 728.825)
															{
																*spatiality = AP_NONCONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 728.825)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
													if (avgdist >= 7.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 1526.705)
												{
													if (avgdist < 12.5)
													{
														if (timediff < 2151.725)
														{
															if (avgdist < 10.5)
															{
																if (timediff < 1651.1100000000001)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 1651.1100000000001)
																{
																	if (timediff < 1794.74)
																	{
																		if (avgdist < 9.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (avgdist >= 9.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 1794.74)
																	{
																		if (timediff < 1901.9099999999999)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 1901.9099999999999)
																		{
																			if (timediff < 2117.1099999999997)
																			{
																				if (timediff < 2083.8050000000003)
																				{
																					if (timediff < 2032.5500000000002)
																					{
																						if (avgdist < 9.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																						if (avgdist >= 9.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																					if (timediff >= 2032.5500000000002)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																				if (timediff >= 2083.8050000000003)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 2117.1099999999997)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																	}
																}
															}
															if (avgdist >= 10.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 2151.725)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (avgdist >= 12.5)
													{
														if (timediff < 2573.315)
														{
															if (timediff < 1628.8049999999998)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 1628.8049999999998)
															{
																if (timediff < 2326.0299999999997)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 2326.0299999999997)
																{
																	if (avgdist < 13.5)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (avgdist >= 13.5)
																	{
																		if (timediff < 2469.085)
																		{
																			if (timediff < 2383.935)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (timediff >= 2383.935)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 2469.085)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																}
															}
														}
														if (timediff >= 2573.315)
														{
															if (avgdist < 15.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 15.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
											}
											if (avgdist >= 20.5)
											{
												if (timediff < 1584.25)
												{
													if (timediff < 1123.8200000000002)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 1123.8200000000002)
													{
														if (timediff < 1510.665)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 1510.665)
														{
															if (timediff < 1565.2800000000002)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 1565.2800000000002)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
												if (timediff >= 1584.25)
												{
													if (avgdist < 22.5)
													{
														if (timediff < 2170.1949999999997)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 2170.1949999999997)
														{
															if (timediff < 2593.005)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 2593.005)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
													}
													if (avgdist >= 22.5)
													{
														if (avgdist < 32.5)
														{
															if (timediff < 2371.55)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 2371.55)
															{
																if (avgdist < 24.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (avgdist >= 24.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
														if (avgdist >= 32.5)
														{
															if (timediff < 2080.58)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 2080.58)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
													}
												}
											}
										}
										if (timediff >= 2633.565)
										{
											if (avgdist < 16.5)
											{
												if (avgdist < 10.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 10.5)
												{
													if (timediff < 2722.955)
													{
														if (avgdist < 13.5)
														{
															if (timediff < 2683.44)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 2683.44)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (avgdist >= 13.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 2722.955)
													{
														if (timediff < 3885.4700000000003)
														{
															if (avgdist < 15.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 15.5)
															{
																if (timediff < 3483.9300000000003)
																{
																	if (timediff < 3240.185)
																	{
																		if (timediff < 3034.205)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 3034.205)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 3240.185)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 3483.9300000000003)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
														if (timediff >= 3885.4700000000003)
														{
															if (avgdist < 14.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (avgdist >= 14.5)
															{
																if (timediff < 3889.035)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 3889.035)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
													}
												}
											}
											if (avgdist >= 16.5)
											{
												if (timediff < 4525.0650000000005)
												{
													if (avgdist < 19.5)
													{
														if (timediff < 3193.495)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 3193.495)
														{
															if (timediff < 4465.045)
															{
																if (timediff < 3716.765)
																{
																	if (timediff < 3319.88)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 3319.88)
																	{
																		if (timediff < 3396.75)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 3396.75)
																		{
																			if (timediff < 3400.8900000000003)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 3400.8900000000003)
																			{
																				if (timediff < 3643.6800000000003)
																				{
																					if (timediff < 3616.64)
																					{
																						if (timediff < 3451.83)
																						{
																							if (timediff < 3432.545)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																							if (timediff >= 3432.545)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																						}
																						if (timediff >= 3451.83)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																					if (timediff >= 3616.64)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																				if (timediff >= 3643.6800000000003)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																		}
																	}
																}
																if (timediff >= 3716.765)
																{
																	if (timediff < 3940.855)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 3940.855)
																	{
																		if (timediff < 4079.98)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 4079.98)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
															}
															if (timediff >= 4465.045)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
													if (avgdist >= 19.5)
													{
														if (avgdist < 31.5)
														{
															if (timediff < 3698.95)
															{
																if (timediff < 2668.645)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 2668.645)
																{
																	if (avgdist < 27.5)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (avgdist >= 27.5)
																	{
																		if (timediff < 3621.26)
																		{
																			if (timediff < 3529.92)
																			{
																				if (timediff < 3123.36)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (timediff >= 3123.36)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 3529.92)
																			{
																				if (avgdist < 30.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (avgdist >= 30.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																		}
																		if (timediff >= 3621.26)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																}
															}
															if (timediff >= 3698.95)
															{
																if (avgdist < 24.5)
																{
																	if (timediff < 3990.39)
																	{
																		if (avgdist < 22.5)
																		{
																			if (timediff < 3930.0)
																			{
																				if (timediff < 3915.02)
																				{
																					if (timediff < 3769.085)
																					{
																						if (timediff < 3741.145)
																						{
																							if (timediff < 3720.51)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																							if (timediff >= 3720.51)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_LARGE;
																								return;
																							}
																						}
																						if (timediff >= 3741.145)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 3769.085)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																				if (timediff >= 3915.02)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																			if (timediff >= 3930.0)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																		if (avgdist >= 22.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 3990.39)
																	{
																		if (timediff < 4462.094999999999)
																		{
																			if (timediff < 4308.5)
																			{
																				if (timediff < 4274.645)
																				{
																					if (avgdist < 23.5)
																					{
																						if (avgdist < 21.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																						if (avgdist >= 21.5)
																						{
																							if (timediff < 4250.71)
																							{
																								if (timediff < 4223.514999999999)
																								{
																									if (timediff < 4169.325000000001)
																									{
																										if (timediff < 4016.045)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																										if (timediff >= 4016.045)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																									}
																									if (timediff >= 4169.325000000001)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																								}
																								if (timediff >= 4223.514999999999)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																							if (timediff >= 4250.71)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																						}
																					}
																					if (avgdist >= 23.5)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																				if (timediff >= 4274.645)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 4308.5)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 4462.094999999999)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																}
																if (avgdist >= 24.5)
																{
																	if (timediff < 3904.6)
																	{
																		if (timediff < 3890.4399999999996)
																		{
																			if (avgdist < 28.5)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																			if (avgdist >= 28.5)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 3890.4399999999996)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 3904.6)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
														}
														if (avgdist >= 31.5)
														{
															if (timediff < 3226.24)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 3226.24)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
												if (timediff >= 4525.0650000000005)
												{
													if (avgdist < 26.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (avgdist >= 26.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
										}
									}
									if (avgdist >= 33.5)
									{
										if (timediff < 2437.235)
										{
											if (timediff < 1022.2)
											{
												*spatiality = AP_NONCONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 1022.2)
											{
												if (timediff < 1426.755)
												{
													if (avgdist < 39.0)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (avgdist >= 39.0)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 1426.755)
												{
													if (avgdist < 57.5)
													{
														if (avgdist < 46.0)
														{
															if (timediff < 1980.875)
															{
																if (avgdist < 37.5)
																{
																	if (timediff < 1824.44)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 1824.44)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (avgdist >= 37.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 1980.875)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 46.0)
														{
															if (timediff < 1946.9899999999998)
															{
																if (avgdist < 52.0)
																{
																	*spatiality = AP_NONCONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (avgdist >= 52.0)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 1946.9899999999998)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
													if (avgdist >= 57.5)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
										}
										if (timediff >= 2437.235)
										{
											if (avgdist < 66.5)
											{
												if (timediff < 4395.8099999999995)
												{
													if (avgdist < 34.5)
													{
														if (timediff < 3512.49)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 3512.49)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (avgdist >= 34.5)
													{
														if (timediff < 2616.95)
														{
															if (avgdist < 41.0)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 41.0)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 2616.95)
														{
															if (avgdist < 60.5)
															{
																if (avgdist < 53.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (avgdist >= 53.5)
																{
																	if (timediff < 3575.745)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 3575.745)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
															}
															if (avgdist >= 60.5)
															{
																if (avgdist < 64.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (avgdist >= 64.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
													}
												}
												if (timediff >= 4395.8099999999995)
												{
													if (avgdist < 44.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 44.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
											if (avgdist >= 66.5)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
						}
					}
					if (timediff >= 4625.78)
					{
						if (operation == RT_READ)
						{
							if (avgdist < 58.5)
							{
								if (avgdist < 51.5)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (avgdist >= 51.5)
								{
									if (timediff < 5098.15)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (timediff >= 5098.15)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
							}
							if (avgdist >= 58.5)
							{
								if (timediff < 6372.665)
								{
									if (timediff < 5623.73)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (timediff >= 5623.73)
									{
										if (avgdist < 62.5)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (avgdist >= 62.5)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
								if (timediff >= 6372.665)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
						}
						if (operation != RT_READ)
						{
							if (timediff < 11631.96)
							{
								if (avgdist < 57.5)
								{
									if (timediff < 9397.369999999999)
									{
										if (avgdist < 51.5)
										{
											if (avgdist < 26.5)
											{
												if (timediff < 5494.129999999999)
												{
													if (avgdist < 17.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 17.5)
													{
														if (timediff < 4674.03)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 4674.03)
														{
															if (timediff < 4933.875)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 4933.875)
															{
																if (timediff < 5026.54)
																{
																	if (timediff < 4973.825000000001)
																	{
																		if (timediff < 4957.1900000000005)
																		{
																			if (avgdist < 21.0)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (avgdist >= 21.0)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																		if (timediff >= 4957.1900000000005)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 4973.825000000001)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (timediff >= 5026.54)
																{
																	if (timediff < 5093.8099999999995)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 5093.8099999999995)
																	{
																		if (timediff < 5120.71)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 5120.71)
																		{
																			if (timediff < 5469.375)
																			{
																				if (timediff < 5412.025)
																				{
																					if (avgdist < 20.5)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																					if (avgdist >= 20.5)
																					{
																						if (timediff < 5271.82)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 5271.82)
																						{
																							if (timediff < 5329.395)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																							if (timediff >= 5329.395)
																							{
																								if (avgdist < 23.0)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																								if (avgdist >= 23.0)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_SMALL;
																									return;
																								}
																							}
																						}
																					}
																				}
																				if (timediff >= 5412.025)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 5469.375)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																	}
																}
															}
														}
													}
												}
												if (timediff >= 5494.129999999999)
												{
													if (timediff < 6466.06)
													{
														if (timediff < 6269.17)
														{
															if (avgdist < 20.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (avgdist >= 20.5)
															{
																if (avgdist < 24.5)
																{
																	if (timediff < 5824.005)
																	{
																		if (timediff < 5788.47)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 5788.47)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 5824.005)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (avgdist >= 24.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
														}
														if (timediff >= 6269.17)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 6466.06)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
											if (avgdist >= 26.5)
											{
												if (timediff < 5481.58)
												{
													if (avgdist < 38.5)
													{
														if (avgdist < 28.5)
														{
															if (timediff < 5229.175)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 5229.175)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 28.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (avgdist >= 38.5)
													{
														if (timediff < 5318.025)
														{
															if (timediff < 5283.370000000001)
															{
																if (timediff < 5253.87)
																{
																	if (timediff < 5240.475)
																	{
																		if (avgdist < 44.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (avgdist >= 44.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 5240.475)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 5253.87)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 5283.370000000001)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (timediff >= 5318.025)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
												if (timediff >= 5481.58)
												{
													if (timediff < 8253.244999999999)
													{
														if (timediff < 5495.07)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 5495.07)
														{
															if (timediff < 5549.26)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 5549.26)
															{
																if (timediff < 5574.425)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 5574.425)
																{
																	if (timediff < 5662.73)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 5662.73)
																	{
																		if (timediff < 5708.505)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 5708.505)
																		{
																			if (avgdist < 45.5)
																			{
																				if (avgdist < 34.5)
																				{
																					if (timediff < 8133.185)
																					{
																						if (timediff < 6296.360000000001)
																						{
																							if (timediff < 5998.635)
																							{
																								if (avgdist < 30.5)
																								{
																									if (timediff < 5861.4349999999995)
																									{
																										if (timediff < 5799.805)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																										if (timediff >= 5799.805)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																									}
																									if (timediff >= 5861.4349999999995)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																								}
																								if (avgdist >= 30.5)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																							if (timediff >= 5998.635)
																							{
																								if (timediff < 6115.325)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_SMALL;
																									return;
																								}
																								if (timediff >= 6115.325)
																								{
																									if (timediff < 6231.014999999999)
																									{
																										if (timediff < 6146.969999999999)
																										{
																											if (timediff < 6129.67)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_LARGE;
																												return;
																											}
																											if (timediff >= 6129.67)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_SMALL;
																												return;
																											}
																										}
																										if (timediff >= 6146.969999999999)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																									}
																									if (timediff >= 6231.014999999999)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																								}
																							}
																						}
																						if (timediff >= 6296.360000000001)
																						{
																							if (avgdist < 33.5)
																							{
																								if (timediff < 7829.83)
																								{
																									if (avgdist < 30.5)
																									{
																										if (timediff < 6512.094999999999)
																										{
																											if (timediff < 6401.615)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_LARGE;
																												return;
																											}
																											if (timediff >= 6401.615)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_SMALL;
																												return;
																											}
																										}
																										if (timediff >= 6512.094999999999)
																										{
																											if (timediff < 6893.715)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_LARGE;
																												return;
																											}
																											if (timediff >= 6893.715)
																											{
																												if (timediff < 6965.514999999999)
																												{
																													*spatiality = AP_CONTIG;
																													*reqsize = AP_SMALL;
																													return;
																												}
																												if (timediff >= 6965.514999999999)
																												{
																													*spatiality = AP_CONTIG;
																													*reqsize = AP_LARGE;
																													return;
																												}
																											}
																										}
																									}
																									if (avgdist >= 30.5)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																								}
																								if (timediff >= 7829.83)
																								{
																									if (timediff < 7851.49)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																									if (timediff >= 7851.49)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																								}
																							}
																							if (avgdist >= 33.5)
																							{
																								if (timediff < 7584.08)
																								{
																									if (timediff < 6407.334999999999)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																									if (timediff >= 6407.334999999999)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																								}
																								if (timediff >= 7584.08)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																						}
																					}
																					if (timediff >= 8133.185)
																					{
																						if (avgdist < 30.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (avgdist >= 30.5)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																				}
																				if (avgdist >= 34.5)
																				{
																					if (timediff < 7740.135)
																					{
																						if (timediff < 7680.97)
																						{
																							if (timediff < 7037.135)
																							{
																								if (timediff < 6696.43)
																								{
																									if (timediff < 6538.06)
																									{
																										if (timediff < 6481.4400000000005)
																										{
																											if (avgdist < 38.5)
																											{
																												if (timediff < 5902.095)
																												{
																													*spatiality = AP_CONTIG;
																													*reqsize = AP_LARGE;
																													return;
																												}
																												if (timediff >= 5902.095)
																												{
																													if (timediff < 6039.305)
																													{
																														*spatiality = AP_CONTIG;
																														*reqsize = AP_SMALL;
																														return;
																													}
																													if (timediff >= 6039.305)
																													{
																														*spatiality = AP_CONTIG;
																														*reqsize = AP_LARGE;
																														return;
																													}
																												}
																											}
																											if (avgdist >= 38.5)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_LARGE;
																												return;
																											}
																										}
																										if (timediff >= 6481.4400000000005)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																									}
																									if (timediff >= 6538.06)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																								}
																								if (timediff >= 6696.43)
																								{
																									if (avgdist < 42.5)
																									{
																										if (timediff < 6711.9400000000005)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																										if (timediff >= 6711.9400000000005)
																										{
																											if (timediff < 7010.075)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_LARGE;
																												return;
																											}
																											if (timediff >= 7010.075)
																											{
																												*spatiality = AP_CONTIG;
																												*reqsize = AP_SMALL;
																												return;
																											}
																										}
																									}
																									if (avgdist >= 42.5)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																								}
																							}
																							if (timediff >= 7037.135)
																							{
																								if (avgdist < 38.5)
																								{
																									if (timediff < 7529.225)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																									if (timediff >= 7529.225)
																									{
																										if (timediff < 7600.04)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																										if (timediff >= 7600.04)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																									}
																								}
																								if (avgdist >= 38.5)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																						}
																						if (timediff >= 7680.97)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 7740.135)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																			}
																			if (avgdist >= 45.5)
																			{
																				if (timediff < 7621.69)
																				{
																					if (timediff < 6252.715)
																					{
																						if (timediff < 6103.885)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 6103.885)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 6252.715)
																					{
																						if (timediff < 6722.594999999999)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 6722.594999999999)
																						{
																							if (timediff < 6870.115)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																							if (timediff >= 6870.115)
																							{
																								if (avgdist < 47.5)
																								{
																									if (timediff < 7104.1)
																									{
																										if (timediff < 6930.215)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																										if (timediff >= 6930.215)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																									}
																									if (timediff >= 7104.1)
																									{
																										if (timediff < 7496.385)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																										if (timediff >= 7496.385)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																									}
																								}
																								if (avgdist >= 47.5)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																						}
																					}
																				}
																				if (timediff >= 7621.69)
																				{
																					if (timediff < 8092.215)
																					{
																						if (timediff < 7964.8)
																						{
																							if (avgdist < 47.5)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																							if (avgdist >= 47.5)
																							{
																								if (timediff < 7654.95)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_SMALL;
																									return;
																								}
																								if (timediff >= 7654.95)
																								{
																									if (timediff < 7782.969999999999)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_LARGE;
																										return;
																									}
																									if (timediff >= 7782.969999999999)
																									{
																										if (timediff < 7896.465)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																										if (timediff >= 7896.465)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																									}
																								}
																							}
																						}
																						if (timediff >= 7964.8)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																					if (timediff >= 8092.215)
																					{
																						if (timediff < 8187.455)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																						if (timediff >= 8187.455)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_SMALL;
																							return;
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
													if (timediff >= 8253.244999999999)
													{
														if (timediff < 9314.275)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 9314.275)
														{
															if (timediff < 9317.625)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 9317.625)
															{
																if (timediff < 9352.265)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 9352.265)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
													}
												}
											}
										}
										if (avgdist >= 51.5)
										{
											if (timediff < 6571.110000000001)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 6571.110000000001)
											{
												if (timediff < 8372.76)
												{
													if (timediff < 7107.0)
													{
														if (timediff < 7034.915)
														{
															if (timediff < 6813.55)
															{
																if (timediff < 6645.115)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 6645.115)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 6813.55)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 7034.915)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 7107.0)
													{
														if (timediff < 7814.82)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 7814.82)
														{
															if (timediff < 7828.639999999999)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 7828.639999999999)
															{
																if (timediff < 7988.615)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 7988.615)
																{
																	if (timediff < 8139.76)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 8139.76)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
														}
													}
												}
												if (timediff >= 8372.76)
												{
													if (timediff < 8662.01)
													{
														if (timediff < 8478.935000000001)
														{
															if (timediff < 8392.67)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 8392.67)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 8478.935000000001)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 8662.01)
													{
														if (timediff < 8805.73)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 8805.73)
														{
															if (timediff < 8869.52)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 8869.52)
															{
																if (timediff < 8934.84)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 8934.84)
																{
																	if (timediff < 9058.5)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 9058.5)
																	{
																		if (timediff < 9175.29)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (timediff >= 9175.29)
																		{
																			if (timediff < 9233.529999999999)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 9233.529999999999)
																			{
																				if (timediff < 9297.72)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (timediff >= 9297.72)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
									if (timediff >= 9397.369999999999)
									{
										if (avgdist < 56.5)
										{
											if (avgdist < 43.5)
											{
												if (timediff < 10348.645)
												{
													if (timediff < 9660.689999999999)
													{
														if (timediff < 9642.0)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 9642.0)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 9660.689999999999)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (timediff >= 10348.645)
												{
													if (timediff < 10389.53)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 10389.53)
													{
														if (avgdist < 40.5)
														{
															if (avgdist < 36.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (avgdist >= 36.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 40.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
											if (avgdist >= 43.5)
											{
												if (timediff < 10170.555)
												{
													if (timediff < 10144.265)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 10144.265)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 10170.555)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
										}
										if (avgdist >= 56.5)
										{
											if (timediff < 11366.69)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 11366.69)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
								if (avgdist >= 57.5)
								{
									if (timediff < 6731.955)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (timediff >= 6731.955)
									{
										if (timediff < 10576.279999999999)
										{
											if (avgdist < 62.5)
											{
												if (timediff < 9236.055)
												{
													if (timediff < 8553.195)
													{
														if (timediff < 7663.405000000001)
														{
															if (avgdist < 60.5)
															{
																if (timediff < 7543.110000000001)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 7543.110000000001)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (avgdist >= 60.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (timediff >= 7663.405000000001)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 8553.195)
													{
														if (timediff < 8675.404999999999)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 8675.404999999999)
														{
															if (timediff < 8803.474999999999)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
															if (timediff >= 8803.474999999999)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
													}
												}
												if (timediff >= 9236.055)
												{
													if (timediff < 9577.83)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 9577.83)
													{
														if (timediff < 9692.835)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 9692.835)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
											if (avgdist >= 62.5)
											{
												if (timediff < 7012.83)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 7012.83)
												{
													if (timediff < 10279.075)
													{
														if (timediff < 10137.61)
														{
															if (timediff < 9969.869999999999)
															{
																if (timediff < 7142.14)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 7142.14)
																{
																	if (timediff < 9596.445)
																	{
																		if (timediff < 9476.51)
																		{
																			if (timediff < 9370.505000000001)
																			{
																				if (timediff < 9230.395)
																				{
																					if (timediff < 9188.0)
																					{
																						if (timediff < 9062.810000000001)
																						{
																							if (timediff < 8985.745)
																							{
																								if (timediff < 8684.605)
																								{
																									if (timediff < 8498.595000000001)
																									{
																										if (timediff < 8428.255000000001)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_SMALL;
																											return;
																										}
																										if (timediff >= 8428.255000000001)
																										{
																											*spatiality = AP_CONTIG;
																											*reqsize = AP_LARGE;
																											return;
																										}
																									}
																									if (timediff >= 8498.595000000001)
																									{
																										*spatiality = AP_CONTIG;
																										*reqsize = AP_SMALL;
																										return;
																									}
																								}
																								if (timediff >= 8684.605)
																								{
																									*spatiality = AP_CONTIG;
																									*reqsize = AP_LARGE;
																									return;
																								}
																							}
																							if (timediff >= 8985.745)
																							{
																								*spatiality = AP_CONTIG;
																								*reqsize = AP_SMALL;
																								return;
																							}
																						}
																						if (timediff >= 9062.810000000001)
																						{
																							*spatiality = AP_CONTIG;
																							*reqsize = AP_LARGE;
																							return;
																						}
																					}
																					if (timediff >= 9188.0)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																				if (timediff >= 9230.395)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 9370.505000000001)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 9476.51)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 9596.445)
																	{
																		if (avgdist < 68.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (avgdist >= 68.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																}
															}
															if (timediff >= 9969.869999999999)
															{
																if (timediff < 10114.275)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 10114.275)
																{
																	if (timediff < 10124.82)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 10124.82)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
														}
														if (timediff >= 10137.61)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 10279.075)
													{
														if (avgdist < 64.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (avgdist >= 64.5)
														{
															if (timediff < 10433.235)
															{
																if (timediff < 10402.970000000001)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 10402.970000000001)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 10433.235)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
													}
												}
											}
										}
										if (timediff >= 10576.279999999999)
										{
											if (timediff < 11384.3)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 11384.3)
											{
												if (timediff < 11453.314999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 11453.314999999999)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
									}
								}
							}
							if (timediff >= 11631.96)
							{
								if (timediff < 13714.585)
								{
									if (avgdist < 63.5)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (avgdist >= 63.5)
									{
										if (timediff < 12023.904999999999)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 12023.904999999999)
										{
											if (timediff < 13497.43)
											{
												if (timediff < 13176.08)
												{
													if (timediff < 13056.685000000001)
													{
														if (timediff < 12973.61)
														{
															if (timediff < 12793.380000000001)
															{
																if (timediff < 12727.825)
																{
																	if (timediff < 12601.275)
																	{
																		if (timediff < 12494.21)
																		{
																			if (timediff < 12324.9)
																			{
																				if (avgdist < 70.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																				if (avgdist >= 70.5)
																				{
																					if (timediff < 12180.349999999999)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																					if (timediff >= 12180.349999999999)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																				}
																			}
																			if (timediff >= 12324.9)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																		}
																		if (timediff >= 12494.21)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 12601.275)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 12727.825)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 12793.380000000001)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (timediff >= 12973.61)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 13056.685000000001)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 13176.08)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 13497.43)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
									}
								}
								if (timediff >= 13714.585)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
						}
					}
				}
				if (avgdist >= 74.5)
				{
					if (timediff < 3410.965)
					{
						if (timediff < 31.345)
						{
							*spatiality = AP_NONCONTIG;
							*reqsize = AP_LARGE;
							return;
						}
						if (timediff >= 31.345)
						{
							*spatiality = AP_NONCONTIG;
							*reqsize = AP_SMALL;
							return;
						}
					}
					if (timediff >= 3410.965)
					{
						if (timediff < 16284.185)
						{
							if (timediff < 5002.174999999999)
							{
								if (avgdist < 116.5)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (avgdist >= 116.5)
								{
									*spatiality = AP_NONCONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
							if (timediff >= 5002.174999999999)
							{
								if (operation == RT_READ)
								{
									if (timediff < 9047.805)
									{
										if (timediff < 8038.725)
										{
											if (timediff < 6110.245)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 6110.245)
											{
												if (timediff < 6171.3150000000005)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 6171.3150000000005)
												{
													if (avgdist < 75.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 75.5)
													{
														if (timediff < 6777.79)
														{
															if (timediff < 6408.03)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (timediff >= 6408.03)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 6777.79)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
											}
										}
										if (timediff >= 8038.725)
										{
											if (avgdist < 87.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (avgdist >= 87.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
									}
									if (timediff >= 9047.805)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
								if (operation != RT_READ)
								{
									if (timediff < 9878.619999999999)
									{
										if (timediff < 5897.0)
										{
											if (avgdist < 91.5)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (avgdist >= 91.5)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
										if (timediff >= 5897.0)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (timediff >= 9878.619999999999)
									{
										if (avgdist < 92.5)
										{
											if (timediff < 13504.725)
											{
												if (timediff < 13071.065)
												{
													if (timediff < 12429.765)
													{
														if (timediff < 9985.064999999999)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 9985.064999999999)
														{
															if (timediff < 12310.46)
															{
																if (timediff < 10112.650000000001)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
																if (timediff >= 10112.650000000001)
																{
																	if (timediff < 10405.7)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 10405.7)
																	{
																		if (timediff < 11922.29)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 11922.29)
																		{
																			if (timediff < 12046.875)
																			{
																				if (avgdist < 78.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (avgdist >= 78.5)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_LARGE;
																					return;
																				}
																			}
																			if (timediff >= 12046.875)
																			{
																				if (timediff < 12146.78)
																				{
																					*spatiality = AP_CONTIG;
																					*reqsize = AP_SMALL;
																					return;
																				}
																				if (timediff >= 12146.78)
																				{
																					if (timediff < 12248.005000000001)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_LARGE;
																						return;
																					}
																					if (timediff >= 12248.005000000001)
																					{
																						*spatiality = AP_CONTIG;
																						*reqsize = AP_SMALL;
																						return;
																					}
																				}
																			}
																		}
																	}
																}
															}
															if (timediff >= 12310.46)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
													}
													if (timediff >= 12429.765)
													{
														if (avgdist < 87.5)
														{
															if (timediff < 12879.39)
															{
																if (timediff < 12717.18)
																{
																	if (timediff < 12487.975)
																	{
																		if (avgdist < 81.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (avgdist >= 81.5)
																		{
																			if (timediff < 12454.66)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_SMALL;
																				return;
																			}
																			if (timediff >= 12454.66)
																			{
																				*spatiality = AP_CONTIG;
																				*reqsize = AP_LARGE;
																				return;
																			}
																		}
																	}
																	if (timediff >= 12487.975)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 12717.18)
																{
																	if (timediff < 12841.085)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																	if (timediff >= 12841.085)
																	{
																		if (avgdist < 81.0)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (avgdist >= 81.0)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																}
															}
															if (timediff >= 12879.39)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 87.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
												if (timediff >= 13071.065)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (timediff >= 13504.725)
											{
												if (avgdist < 80.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 80.5)
												{
													if (timediff < 13677.475)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 13677.475)
													{
														if (timediff < 15302.5)
														{
															if (timediff < 15256.765)
															{
																if (avgdist < 91.5)
																{
																	if (timediff < 15079.55)
																	{
																		if (timediff < 15028.055)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																		if (timediff >= 15028.055)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																	}
																	if (timediff >= 15079.55)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (avgdist >= 91.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 15256.765)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 15302.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
											}
										}
										if (avgdist >= 92.5)
										{
											if (timediff < 15081.225)
											{
												if (timediff < 11326.92)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
												if (timediff >= 11326.92)
												{
													if (timediff < 15042.105)
													{
														if (timediff < 13127.61)
														{
															if (timediff < 12757.485)
															{
																if (avgdist < 95.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (avgdist >= 95.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
															if (timediff >= 12757.485)
															{
																if (timediff < 12873.055)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
																if (timediff >= 12873.055)
																{
																	if (timediff < 12964.98)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 12964.98)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
															}
														}
														if (timediff >= 13127.61)
														{
															if (avgdist < 96.5)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (avgdist >= 96.5)
															{
																if (avgdist < 106.5)
																{
																	if (timediff < 14548.475)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																	if (timediff >= 14548.475)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_LARGE;
																		return;
																	}
																}
																if (avgdist >= 106.5)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_SMALL;
																	return;
																}
															}
														}
													}
													if (timediff >= 15042.105)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
											if (timediff >= 15081.225)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
									}
								}
							}
						}
						if (timediff >= 16284.185)
						{
							if (timediff < 19342.690000000002)
							{
								if (avgdist < 100.5)
								{
									if (avgdist < 89.5)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (avgdist >= 89.5)
									{
										if (timediff < 18618.64)
										{
											if (timediff < 18316.26)
											{
												if (timediff < 16690.71)
												{
													if (timediff < 16470.62)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 16470.62)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 16690.71)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (timediff >= 18316.26)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
										if (timediff >= 18618.64)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
								}
								if (avgdist >= 100.5)
								{
									if (timediff < 17909.14)
									{
										if (timediff < 17101.86)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 17101.86)
										{
											if (avgdist < 111.0)
											{
												if (timediff < 17759.055)
												{
													if (timediff < 17661.77)
													{
														if (avgdist < 106.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (avgdist >= 106.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (timediff >= 17661.77)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (timediff >= 17759.055)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (avgdist >= 111.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
									if (timediff >= 17909.14)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
								}
							}
							if (timediff >= 19342.690000000002)
							{
								if (avgdist < 111.5)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (avgdist >= 111.5)
								{
									if (timediff < 22567.725)
									{
										if (timediff < 20726.84)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 20726.84)
										{
											if (timediff < 22045.855)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
											if (timediff >= 22045.855)
											{
												if (timediff < 22494.29)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 22494.29)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
									}
									if (timediff >= 22567.725)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
							}
						}
					}
				}
			}
			if (avgdist >= 124.5)
			{
				if (avgdist < 1341.5)
				{
					if (timediff < 61.49)
					{
						if (avgdist < 924.0)
						{
							*spatiality = AP_NONCONTIG;
							*reqsize = AP_LARGE;
							return;
						}
						if (avgdist >= 924.0)
						{
							if (timediff < 35.965)
							{
								if (timediff < 13.835)
								{
									if (avgdist < 1016.0)
									{
										if (operation == RT_READ)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (operation != RT_READ)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
									if (avgdist >= 1016.0)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
								if (timediff >= 13.835)
								{
									if (timediff < 22.35)
									{
										if (avgdist < 1189.5)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (avgdist >= 1189.5)
										{
											if (operation == RT_READ)
											{
												*spatiality = AP_NONCONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (operation != RT_READ)
											{
												if (timediff < 17.509999999999998)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 17.509999999999998)
												{
													if (timediff < 18.41)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 18.41)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
										}
									}
									if (timediff >= 22.35)
									{
										if (operation == RT_READ)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (operation != RT_READ)
										{
											if (avgdist < 1259.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (avgdist >= 1259.0)
											{
												*spatiality = AP_NONCONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
							if (timediff >= 35.965)
							{
								*spatiality = AP_NONCONTIG;
								*reqsize = AP_LARGE;
								return;
							}
						}
					}
					if (timediff >= 61.49)
					{
						if (timediff < 10098.985)
						{
							if (timediff < 7240.41)
							{
								if (timediff < 80.18)
								{
									if (avgdist < 425.5)
									{
										*spatiality = AP_NONCONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (avgdist >= 425.5)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
								if (timediff >= 80.18)
								{
									*spatiality = AP_NONCONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
							if (timediff >= 7240.41)
							{
								if (avgdist < 207.5)
								{
									if (operation == RT_READ)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (operation != RT_READ)
									{
										if (timediff < 8995.68)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 8995.68)
										{
											*spatiality = AP_NONCONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
								if (avgdist >= 207.5)
								{
									*spatiality = AP_NONCONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
						}
						if (timediff >= 10098.985)
						{
							if (avgdist < 279.0)
							{
								if (timediff < 25681.68)
								{
									if (timediff < 21227.475)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_SMALL;
										return;
									}
									if (timediff >= 21227.475)
									{
										if (timediff < 21282.22)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (timediff >= 21282.22)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
								}
								if (timediff >= 25681.68)
								{
									if (timediff < 28567.83)
									{
										if (timediff < 27490.059999999998)
										{
											if (timediff < 26308.025)
											{
												if (avgdist < 135.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 135.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (timediff >= 26308.025)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
										if (timediff >= 27490.059999999998)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
									}
									if (timediff >= 28567.83)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
								}
							}
							if (avgdist >= 279.0)
							{
								if (timediff < 24100.445)
								{
									*spatiality = AP_NONCONTIG;
									*reqsize = AP_SMALL;
									return;
								}
								if (timediff >= 24100.445)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
							}
						}
					}
				}
				if (avgdist >= 1341.5)
				{
					if (avgdist < 4906.5)
					{
						if (timediff < 501.385)
						{
							if (timediff < 32.43)
							{
								if (avgdist < 2333.5)
								{
									if (timediff < 14.9)
									{
										if (avgdist < 1971.5)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
										if (avgdist >= 1971.5)
										{
											if (timediff < 10.469999999999999)
											{
												if (timediff < 9.86)
												{
													if (timediff < 8.46)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 8.46)
													{
														if (operation == RT_READ)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (operation != RT_READ)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
												}
												if (timediff >= 9.86)
												{
													if (avgdist < 2213.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (avgdist >= 2213.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
											if (timediff >= 10.469999999999999)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
									if (timediff >= 14.9)
									{
										if (avgdist < 1673.5)
										{
											if (avgdist < 1443.0)
											{
												if (timediff < 21.425)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 21.425)
												{
													*spatiality = AP_NONCONTIG;
													*reqsize = AP_LARGE;
													return;
												}
											}
											if (avgdist >= 1443.0)
											{
												if (operation == RT_READ)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (operation != RT_READ)
												{
													if (timediff < 19.395)
													{
														if (avgdist < 1515.0)
														{
															if (timediff < 17.11)
															{
																if (timediff < 15.8)
																{
																	if (timediff < 15.465)
																	{
																		if (avgdist < 1503.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_LARGE;
																			return;
																		}
																		if (avgdist >= 1503.5)
																		{
																			*spatiality = AP_CONTIG;
																			*reqsize = AP_SMALL;
																			return;
																		}
																	}
																	if (timediff >= 15.465)
																	{
																		*spatiality = AP_CONTIG;
																		*reqsize = AP_SMALL;
																		return;
																	}
																}
																if (timediff >= 15.8)
																{
																	*spatiality = AP_CONTIG;
																	*reqsize = AP_LARGE;
																	return;
																}
															}
															if (timediff >= 17.11)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
														}
														if (avgdist >= 1515.0)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
													}
													if (timediff >= 19.395)
													{
														if (timediff < 21.27)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 21.27)
														{
															*spatiality = AP_NONCONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
										}
										if (avgdist >= 1673.5)
										{
											if (operation == RT_READ)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
											if (operation != RT_READ)
											{
												if (avgdist < 1778.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 1778.5)
												{
													if (avgdist < 2198.0)
													{
														*spatiality = AP_NONCONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 2198.0)
													{
														if (timediff < 17.09)
														{
															*spatiality = AP_NONCONTIG;
															*reqsize = AP_LARGE;
															return;
														}
														if (timediff >= 17.09)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
										}
									}
								}
								if (avgdist >= 2333.5)
								{
									if (timediff < 12.485)
									{
										if (avgdist < 2630.0)
										{
											if (timediff < 10.415)
											{
												if (avgdist < 2380.5)
												{
													if (timediff < 9.785)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 9.785)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
												if (avgdist >= 2380.5)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
											if (timediff >= 10.415)
											{
												if (timediff < 12.04)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 12.04)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_SMALL;
													return;
												}
											}
										}
										if (avgdist >= 2630.0)
										{
											if (avgdist < 4665.0)
											{
												if (timediff < 10.27)
												{
													if (avgdist < 3990.5)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 3990.5)
													{
														if (avgdist < 4212.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (avgdist >= 4212.5)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
												if (timediff >= 10.27)
												{
													if (operation == RT_READ)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (operation != RT_READ)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
											}
											if (avgdist >= 4665.0)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_SMALL;
												return;
											}
										}
									}
									if (timediff >= 12.485)
									{
										if (timediff < 24.009999999999998)
										{
											if (timediff < 14.43)
											{
												if (avgdist < 3138.0)
												{
													if (timediff < 13.68)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (timediff >= 13.68)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
												}
												if (avgdist >= 3138.0)
												{
													if (avgdist < 4599.0)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
													if (avgdist >= 4599.0)
													{
														if (timediff < 13.92)
														{
															if (operation == RT_READ)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_SMALL;
																return;
															}
															if (operation != RT_READ)
															{
																*spatiality = AP_CONTIG;
																*reqsize = AP_LARGE;
																return;
															}
														}
														if (timediff >= 13.92)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
												}
											}
											if (timediff >= 14.43)
											{
												if (timediff < 22.455)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (timediff >= 22.455)
												{
													if (avgdist < 3978.0)
													{
														if (timediff < 23.175)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_SMALL;
															return;
														}
														if (timediff >= 23.175)
														{
															*spatiality = AP_CONTIG;
															*reqsize = AP_LARGE;
															return;
														}
													}
													if (avgdist >= 3978.0)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
										}
										if (timediff >= 24.009999999999998)
										{
											if (avgdist < 4255.5)
											{
												if (avgdist < 3611.0)
												{
													*spatiality = AP_CONTIG;
													*reqsize = AP_LARGE;
													return;
												}
												if (avgdist >= 3611.0)
												{
													if (timediff < 31.095)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_SMALL;
														return;
													}
													if (timediff >= 31.095)
													{
														*spatiality = AP_CONTIG;
														*reqsize = AP_LARGE;
														return;
													}
												}
											}
											if (avgdist >= 4255.5)
											{
												*spatiality = AP_CONTIG;
												*reqsize = AP_LARGE;
												return;
											}
										}
									}
								}
							}
							if (timediff >= 32.43)
							{
								if (avgdist < 1728.0)
								{
									*spatiality = AP_NONCONTIG;
									*reqsize = AP_LARGE;
									return;
								}
								if (avgdist >= 1728.0)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
						}
						if (timediff >= 501.385)
						{
							*spatiality = AP_NONCONTIG;
							*reqsize = AP_SMALL;
							return;
						}
					}
					if (avgdist >= 4906.5)
					{
						if (operation == RT_READ)
						{
							if (avgdist < 5387.0)
							{
								if (timediff < 26.525)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_SMALL;
									return;
								}
								if (timediff >= 26.525)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
							if (avgdist >= 5387.0)
							{
								if (avgdist < 5445.5)
								{
									if (timediff < 19.14)
									{
										*spatiality = AP_CONTIG;
										*reqsize = AP_LARGE;
										return;
									}
									if (timediff >= 19.14)
									{
										if (timediff < 29.925)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_SMALL;
											return;
										}
										if (timediff >= 29.925)
										{
											*spatiality = AP_CONTIG;
											*reqsize = AP_LARGE;
											return;
										}
									}
								}
								if (avgdist >= 5445.5)
								{
									*spatiality = AP_CONTIG;
									*reqsize = AP_LARGE;
									return;
								}
							}
						}
						if (operation != RT_READ)
						{
							*spatiality = AP_CONTIG;
							*reqsize = AP_SMALL;
							return;
						}
					}
				}
			}
		}
	}
}
